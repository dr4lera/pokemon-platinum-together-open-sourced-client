// SPDX-License-Identifier: GPL-3.0-or-later
// @category Community.Reference
// Offline pseudocode export. No recovered reference or player data belongs here.
import com.google.gson.JsonObject;
import com.google.gson.JsonParser;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.util.headless.HeadlessScript;
import ghidra.framework.Application;
import ghidra.framework.model.DomainFile;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import java.io.BufferedWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.security.MessageDigest;
import java.time.Instant;
import java.util.HexFormat;

public class ExportReferenceV2 extends HeadlessScript {
    private static String hash(byte[] bytes) throws Exception {
        return HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(bytes));
    }

    private static String hash(Path path) throws Exception { return hash(Files.readAllBytes(path)); }

    private static void write(Path path, String text) throws Exception {
        Path temporary = path.resolveSibling(path.getFileName() + ".tmp");
        Files.writeString(temporary, text, StandardCharsets.UTF_8);
        Files.move(temporary, path, StandardCopyOption.REPLACE_EXISTING, StandardCopyOption.ATOMIC_MOVE);
    }

    private static JsonObject read(Path path) throws Exception {
        return JsonParser.parseString(Files.readString(path, StandardCharsets.UTF_8)).getAsJsonObject();
    }

    private static String string(JsonObject object, String key) {
        return object.get(key).getAsString();
    }

    private static void require(boolean condition, String reason) {
        if (!condition) throw new IllegalStateException(reason);
    }

    private static long count(FunctionIterator functions) {
        long count = 0;
        while (functions.hasNext()) { functions.next(); count++; }
        return count;
    }

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        require(args.length == 4, "Expected phase, output directory, run UUID and timeout seconds");
        String phase = args[0], runId = args[2];
        require(phase.equals("analyze") || phase.equals("export"), "Unknown phase");
        require(runId.matches("[a-f0-9-]{36}"), "Invalid run UUID");
        int timeout = Integer.parseInt(args[3]);
        require(timeout >= 1 && timeout <= 120, "Timeout must be 1..120 seconds");
        Path output = Path.of(args[1]).toAbsolutePath();
        Path run = output.resolve("runs").resolve(runId);
        Files.createDirectories(run);
        JsonObject manifest = read(output.resolve("reference-v2.json"));
        String sourceHash = currentProgram.getExecutableSHA256();
        require(string(manifest, "artifactSha256").equals(sourceHash), "Retained program artifact mismatch");
        require(string(manifest, "exporterSha256").equals(hash(Path.of(getSourceFile().getAbsolutePath()))), "Exporter changed");
        require(string(manifest, "ghidraVersion").equals(Application.getApplicationVersion()), "Ghidra version mismatch");
        require(!analysisTimeoutOccurred(), "Analysis timed out; retain the project, do not mark a saved checkpoint");

        if (phase.equals("analyze")) {
            require(isImporting(), "Analysis checkpoint must be a new import");
            JsonObject checked = new JsonObject();
            checked.addProperty("runId", runId);
            checked.addProperty("phase", "analysis_checked_in_memory");
            checked.addProperty("artifactSha256", sourceHash);
            checked.addProperty("originalSource", false);
            checked.addProperty("saved", false);
            write(run.resolve("analysis.json"), checked + "\n");
            // The headless importer saves after this script returns. The wrapper
            // checks that distinct save result before opening the export pass.
            return;
        }

        require(!isImporting(), "Export must open the persisted program");
        require(manifest.has("savedProjectFingerprint"), "No durable analysis checkpoint");
        long total = currentProgram.getFunctionManager().getFunctionCount();
        long internal = count(currentProgram.getFunctionManager().getFunctions(true));
        long external = count(currentProgram.getFunctionManager().getExternalFunctions());
        long processed = 0, processedExternal = 0, recovered = 0, cached = 0, failed = 0, diagnostics = 0;
        String state = "exporting", failure = null;
        String started = Instant.now().toString();
        DecompInterface decompiler = new DecompInterface();
        JsonObject profile = new JsonObject();
        JsonObject coverage = new JsonObject();
        coverage.addProperty("schema", 2);
        coverage.addProperty("runId", runId);
        coverage.addProperty("phase", state);
        coverage.addProperty("artifactSha256", sourceHash);
        coverage.addProperty("functionTimeoutSeconds", timeout);
        coverage.addProperty("startedAt", started);
        coverage.addProperty("originalSource", false);
        coverage.addProperty("runtimeVerified", false);
        coverage.addProperty("allRecognizedInternalFunctionsExported", false);
        Path partial = run.resolve("functions.partial.jsonl");
        require(!Files.exists(partial) && !Files.exists(run.resolve("functions.jsonl"))
            && !Files.exists(run.resolve("coverage.json")), "Run ID already used");
        write(run.resolve("coverage.json"), coverage + "\n");
        try {
            require(internal + external == total, "Function denominator mismatch before export");
            DomainFile domain = currentProgram.getDomainFile();
            require(domain != null && domain.getFileID() != null && domain.getLastModifiedTime() > 0, "Program has no saved domain file");
            decompiler.toggleCCode(true);
            decompiler.toggleSyntaxTree(false);
            decompiler.setSimplificationStyle("decompile");
            require(decompiler.openProgram(currentProgram), decompiler.getLastMessage());
            for (String key : new String[] { "artifactSha256", "exporterSha256", "workflowSha256", "ghidraVersion", "decompilerBinarySha256", "savedProjectFingerprintAlgorithm", "savedProjectFingerprint" }) {
                profile.addProperty(key, string(manifest, key));
            }
            profile.addProperty("schema", 2);
            profile.addProperty("decompilerVersion", decompiler.getMajorVersion() + "." + decompiler.getMinorVersion());
            profile.addProperty("language", currentProgram.getLanguageID().toString());
            profile.addProperty("compiler", currentProgram.getCompilerSpec().getCompilerSpecID().toString());
            profile.addProperty("domainFileId", domain.getFileID());
            profile.addProperty("domainVersion", domain.getVersion());
            profile.addProperty("domainLastModifiedTime", domain.getLastModifiedTime());
            profile.addProperty("outputProfile", "function-c-v2;decompile;cCode=true;syntaxTree=false;default-options");
            String profileHash = hash(profile.toString().getBytes(StandardCharsets.UTF_8));
            coverage.addProperty("profileSha256", profileHash);
            Path cache = output.resolve("cache");
            Files.createDirectories(cache.resolve("functions"));
            Path profileFile = cache.resolve("profile.json");
            if (Files.exists(profileFile)) require(read(profileFile).equals(profile), "Cache profile mismatch; use a fresh output directory");
            else write(profileFile, profile + "\n");
            write(run.resolve("profile.json"), profile + "\n");
            try (BufferedWriter index = Files.newBufferedWriter(partial, StandardCharsets.UTF_8)) {
                FunctionIterator functions = currentProgram.getFunctionManager().getFunctions(true);
                while (functions.hasNext()) {
                    monitor.checkCancelled();
                    Function function = functions.next();
                    require(!function.isExternal(), "Unexpected external function in internal iterator");
                    String address = function.getEntryPoint().toString();
                    String fileName = hash(address.getBytes(StandardCharsets.UTF_8)) + ".c";
                    Path file = cache.resolve("functions").resolve(fileName);
                    Path metadata = file.resolveSibling(fileName + ".json");
                    String header = "/* Recovered pseudocode; not original or necessarily buildable source.\n * Artifact SHA-256: "
                        + sourceHash + "\n * Entry: " + address + "\n * Export profile SHA-256: " + profileHash + "\n */\n";
                    String status = "failed", digest = null, diagnostic = null;
                    boolean valid = false;
                    if (Files.isRegularFile(file) && Files.isRegularFile(metadata)) {
                        try {
                            JsonObject meta = read(metadata);
                            String text = Files.readString(file, StandardCharsets.UTF_8);
                            valid = profileHash.equals(string(meta, "profileSha256")) && address.equals(string(meta, "address"))
                                && text.startsWith(header) && hash(file).equals(string(meta, "sha256"));
                            if (valid) { digest = string(meta, "sha256"); diagnostic = meta.has("diagnostic") ? string(meta, "diagnostic") : null; }
                        } catch (Exception badCache) { valid = false; }
                    }
                    if (valid) { status = "cached"; cached++; }
                    else {
                        DecompileResults result = decompiler.decompileFunction(function, timeout, monitor);
                        diagnostic = result.getErrorMessage();
                        if (result.decompileCompleted() && result.getDecompiledFunction() != null) {
                            String text = header + result.getDecompiledFunction().getC();
                            digest = hash(text.getBytes(StandardCharsets.UTF_8));
                            JsonObject meta = new JsonObject();
                            meta.addProperty("profileSha256", profileHash);
                            meta.addProperty("address", address);
                            meta.addProperty("sha256", digest);
                            if (diagnostic != null && !diagnostic.isBlank()) meta.addProperty("diagnostic", diagnostic);
                            write(file, text);
                            write(metadata, meta + "\n");
                            status = "recovered"; recovered++;
                        } else { failed++; }
                    }
                    if (diagnostic != null && !diagnostic.isBlank()) diagnostics++;
                    JsonObject row = new JsonObject();
                    row.addProperty("address", address);
                    row.addProperty("name", function.getName(true));
                    row.addProperty("thunk", function.isThunk());
                    row.addProperty("status", status);
                    if (digest != null) { row.addProperty("file", "cache/functions/" + fileName); row.addProperty("sha256", digest); }
                    if (diagnostic != null && !diagnostic.isBlank()) row.addProperty("diagnostic", diagnostic);
                    index.write(row + "\n"); index.flush(); processed++;
                    if (processed % 100 == 0) {
                        JsonObject progress = new JsonObject();
                        progress.addProperty("runId", runId); progress.addProperty("processedInternal", processed);
                        progress.addProperty("totalInternal", internal); progress.addProperty("failed", failed);
                        write(run.resolve("progress.json"), progress + "\n");
                        println("Reference V2 " + processed + "/" + internal + " internal; failed=" + failed);
                    }
                }
                functions = currentProgram.getFunctionManager().getExternalFunctions();
                while (functions.hasNext()) {
                    monitor.checkCancelled();
                    Function function = functions.next();
                    require(function.isExternal(), "Unexpected internal function in external iterator");
                    JsonObject row = new JsonObject();
                    row.addProperty("address", function.getEntryPoint().toString());
                    row.addProperty("name", function.getName(true)); row.addProperty("status", "external");
                    index.write(row + "\n"); processedExternal++;
                }
            }
            require(processed == internal && processedExternal == external && recovered + cached + failed == internal
                && currentProgram.getFunctionManager().getFunctionCount() == total, "Function counts changed or did not reconcile");
            Files.move(partial, run.resolve("functions.jsonl"), StandardCopyOption.ATOMIC_MOVE);
            state = "exported";
        } catch (Exception error) {
            state = monitor.isCancelled() ? "interrupted" : "failed";
            failure = error.toString();
            throw error;
        } finally {
            decompiler.dispose();
            coverage.addProperty("phase", state);
            coverage.addProperty("finishedAt", Instant.now().toString());
            coverage.addProperty("totalFunctions", total); coverage.addProperty("internalFunctions", internal);
            coverage.addProperty("externalFunctions", external); coverage.addProperty("processedInternal", processed);
            coverage.addProperty("processedExternal", processedExternal); coverage.addProperty("recovered", recovered);
            coverage.addProperty("cached", cached); coverage.addProperty("failed", failed);
            coverage.addProperty("functionsWithDiagnostics", diagnostics);
            coverage.addProperty("allRecognizedInternalFunctionsExported", state.equals("exported") && failed == 0);
            if (failure != null) coverage.addProperty("error", failure);
            write(run.resolve("coverage.json"), coverage + "\n");
        }
    }
}
