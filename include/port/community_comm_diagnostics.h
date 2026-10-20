#ifndef COMMUNITY_COMM_DIAGNOSTICS_H
#define COMMUNITY_COMM_DIAGNOSTICS_H

/* Opt-in desktop QA counters only. Never include payload or account data. */
#ifdef SDK_PORT
void CommunityCommDiagnostics_Record(const char *reason, const void *site,
    int a, int b, int c, int d, int e, int f);
#define COMMUNITY_COMM_FAULT(reason, site, a, b, c, d, e, f) \
    CommunityCommDiagnostics_Record(reason, site, a, b, c, d, e, f)
#else
#define COMMUNITY_COMM_FAULT(reason, site, a, b, c, d, e, f) ((void)0)
#endif

#endif
