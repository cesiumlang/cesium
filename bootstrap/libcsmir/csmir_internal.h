/* What stage 0 uses from libcsmir besides the coarse API: it hands over the
 * module it built instead of MIR text. */
#ifndef CS_CSMIR_INTERNAL_H
#define CS_CSMIR_INTERNAL_H

#include "mir.h"

/* The QBE target for a triple or QBE target name; "" or NULL means the
 * machine this runs on. NULL with cs_last_error() set if there is none. */
const MirTarget *csmir_target(const char *triple);

/* Verifies m, writes its QBE IL to "<out_path>.ssa" and runs QBE on it to
 * write assembly to out_path. Returns nonzero on an error, with
 * cs_last_error() set; on success QBE ends the process and this does not
 * return. */
int csmir_emit_qbe(MirModule *m, const MirTarget *t, const char *out_path);

#endif
