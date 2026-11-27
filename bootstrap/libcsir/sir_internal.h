/* What stage 0 uses from libcsir besides the coarse API: it hands over the
 * module it built instead of SIR text. */
#ifndef CS_SIR_INTERNAL_H
#define CS_SIR_INTERNAL_H

#include "sir_core.h"

/* The QBE target for a triple or QBE target name; "" or NULL means the
 * machine this runs on. NULL with cs_last_error() set if there is none. */
const SirTarget *csir_target(const char *triple);

/* Verifies m, writes its QBE IL to "<out_path>.ssa" and runs QBE on it to
 * write assembly to out_path, with variables kept for debuggers if asked.
 * Returns nonzero on an error, with cs_last_error() set; on success QBE ends
 * the process and this does not return. */
int csir_emit_qbe(SirModule *m, const SirTarget *t, const char *out_path, bool variables);

/* Reads a build mode's name (-O): debug, safe, fast or small. Returns false
 * if it is none of them. */
bool csir_opt_mode(const char *name, int *mode);

#endif
