#ifndef GUI_H
#define GUI_H

/* Builds the whole IUP interface, runs the main loop and returns
 * the process exit code. Handles IupOpen/IupClose internally. */
int gui_run(int argc, char **argv);

#endif /* GUI_H */
