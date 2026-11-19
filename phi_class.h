/* phi_class.h - PHIDM NEO extension: phi.class (Lua) switch programs.
 *
 * A switch record with action type 62 carries a program name (a quoted
 * string). When the switch fires, the same-named Lua function defined
 * in the world's phi.class file is executed and control returns to the
 * normal switch processing.
 *
 * class_load()  : (re)load phi.class into a fresh Lua state. Called at
 *                 server startup and by the master spell "Load class".
 * class_call()  : run one program for player n; sx, sy are the switch
 *                 cell coordinates that fired.
 */
#ifndef PHI_CLASS_H
#define PHI_CLASS_H

void class_load(void);
void class_call(int n, const char *prog, int sx, int sy);

#endif /* PHI_CLASS_H */
