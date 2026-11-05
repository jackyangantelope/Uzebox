/*
 * Minimal SDL include shim for compiling CUzeBox's platform-neutral core.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The selected CUzeBox core files do not use SDL symbols, but upstream's
 * shared types.h includes SDL2/SDL.h unconditionally.  Keeping this empty
 * shim local avoids modifying the audited upstream checkout.
 */
#ifndef JACK_YELLOW_CUZEBOX_SDL_SHIM_H
#define JACK_YELLOW_CUZEBOX_SDL_SHIM_H
#endif
