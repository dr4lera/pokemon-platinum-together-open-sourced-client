// SPDX-License-Identifier: GPL-3.0-or-later
// Resumable, execution-free native pseudocode reference export.
// @category Community.Reference
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.util.headless.HeadlessScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import java.io.BufferedWriter;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.security.MessageDigest;
import java.util.HexFormat;

public class ExportReference extends HeadlessScript {
    private static String json(String value) {
        if (value == null) return "null";
        StringBuilder text = new StringBuilder("\"");
        for (char c : value.toCharArray()) {
            if (c == '"' || c == '\\') text.append('\\').append(c);
            else if (c < 32) text.append(String.format("\\u%04x", (int)c));
            else text.append(c);
        }
        return text.append('"').toString();
    }

    private static String digest(byte[] data) throws Exception {
        return HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(data));
    }

    private static void write(Path path, String value) throws Exception {
        Path temporary = path.resolveSibling(path.getFileName() + ".tmp");
        Files.writeString(temporary, value, StandardCharsets.UTF_8);
        Files.move(temporary, path, StandardCopyOption.REPLACE_EXISTING, StandardCopyOption.ATOMIC_MOVE);
    }

    @Override
    protected void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 2) throw new IllegalArgumentException("Expected output directory and per-function timeout seconds");
        Path output = Path.of(args[0]).toAbsolutePath();
        int timeout = Integer.parseInt(args[1]);
        if (timeout < 1 || timeout > 120) throw new IllegalArgumentException("Timeout must be 1..120 seconds");
        Files.createDirectories(output.resolve("functions"));
        if (analysisTimeoutOccurred()) {
            write(output.resolve("coverage.json"), "{\"phase\":\"analysis_timed_out\",\"complete\":false}\n");
            throw new IllegalStateException("Auto-analysis timed out; retain the project and resume analysis before exporting");
        }
        String sourceHash = currentProgram.getExecutableSHA256();
        write(output.resolve("analysis-complete.json"), "{\"sha256\":" + json(sourceHash) + ",\"analysis_timeout\":false}\n");
        DecompInterface decompiler = new DecompInterface();
        decompiler.toggleCCode(true);
        decompiler.toggleSyntaxTree(false);
        decompiler.setSimplificationStyle("decompile");
        if (!decompiler.openProgram(currentProgram)) throw new IllegalStateException(decompiler.getLastMessage());
        long total = currentProgram.getFunctionManager().getFunctionCount();
        long processed = 0, recovered = 0, cached = 0, failed = 0, external = 0;
        boolean finished = false;
        try (BufferedWriter index = Files.newBufferedWriter(output.resolve("functions.jsonl"), StandardCharsets.UTF_8)) {
            FunctionIterator functions = currentProgram.getFunctionManager().getFunctions(true);
            while (functions.hasNext()) {
                monitor.checkCancelled();
                Function function = functions.next();
                String address = function.getEntryPoint().toString();
                String fileName = address.replaceAll("[^a-zA-Z0-9_-]", "_") + ".c";
                Path file = output.resolve("functions").resolve(fileName);
                Path hashFile = file.resolveSibling(fileName + ".sha256");
                String status, error = null, hash = null;
                if (function.isExternal()) {
                    status = "external";
                    external++;
                } else {
                    boolean validCache = Files.isRegularFile(file) && Files.isRegularFile(hashFile);
                    if (validCache) {
                        hash = digest(Files.readAllBytes(file));
                        validCache = hash.equals(Files.readString(hashFile).trim());
                    }
                    if (validCache) {
                        status = "cached";
                        cached++;
                    } else {
                        DecompileResults result = decompiler.decompileFunction(function, timeout, monitor);
                        if (result.decompileCompleted() && result.getDecompiledFunction() != null) {
                            String text = "/* Recovered pseudocode; not original or necessarily buildable source.\n"
                                + " * Artifact SHA-256: " + sourceHash + "\n * Entry: " + address + "\n */\n"
                                + result.getDecompiledFunction().getC();
                            write(file, text);
                            hash = digest(text.getBytes(StandardCharsets.UTF_8));
                            write(hashFile, hash + "\n");
                            status = "recovered";
                            recovered++;
                        } else {
                            status = "failed";
                            error = result.getErrorMessage();
                            failed++;
                        }
                    }
                }
                index.write("{\"address\":" + json(address) + ",\"name\":" + json(function.getName(true))
                    + ",\"thunk\":" + function.isThunk() + ",\"status\":" + json(status)
                    + ",\"file\":" + json(hash == null ? null : "functions/" + fileName)
                    + ",\"sha256\":" + json(hash) + ",\"error\":" + json(error) + "}\n");
                index.flush();
                processed++;
                if (processed % 100 == 0) {
                    write(output.resolve("progress.json"), "{\"processed\":" + processed + ",\"total\":" + total
                        + ",\"recovered\":" + recovered + ",\"cached\":" + cached + ",\"failed\":" + failed + "}\n");
                    println("Reference export " + processed + "/" + total + "; failed=" + failed);
                }
            }
            finished = true;
        } finally {
            decompiler.dispose();
            write(output.resolve("coverage.json"), "{\"phase\":" + json(finished ? "exported" : "interrupted")
                + ",\"sha256\":" + json(sourceHash) + ",\"processed\":" + processed + ",\"total\":" + total
                + ",\"recovered\":" + recovered + ",\"cached\":" + cached + ",\"external\":" + external
                + ",\"failed\":" + failed + ",\"complete\":" + (finished && failed == 0)
                + ",\"original_source\":false,\"runtime_verified\":false}\n");
        }
    }
}
