/* Modified TinyWM with requested features */

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))

Display *dpy;
Window root;
XFontStruct *font;
GC gc;
Cursor default_cursor;

typedef struct Client {
    Window frame;
    Window window;
    struct Client *next;
} Client;

Client *clients = NULL;

/* Function to create a frame window around a client window */
Window create_frame(Window w) {
    XWindowAttributes attr;
    XGetWindowAttributes(dpy, w, &attr);

    /* Create the frame window */
    Window frame = XCreateSimpleWindow(dpy, root,
        attr.x, attr.y, attr.width, attr.height + FONT_SIZE + 4, BORDER_WIDTH,
        BlackPixel(dpy, DefaultScreen(dpy)),
        WhitePixel(dpy, DefaultScreen(dpy)));

    /* Select events on the frame */
    XSelectInput(dpy, frame, SubstructureRedirectMask | SubstructureNotifyMask |
                 ExposureMask | ButtonPressMask | ButtonReleaseMask |
                 PointerMotionMask | EnterWindowMask | LeaveWindowMask);

    /* Reparent the client window */
    XAddToSaveSet(dpy, w);
    XReparentWindow(dpy, w, frame, 0, FONT_SIZE + 4);

    /* Map the frame */
    XMapWindow(dpy, frame);

    /* Add to client list */
    Client *c = malloc(sizeof(Client));
    c->frame = frame;
    c->window = w;
    c->next = clients;
    clients = c;

    /* Draw decorations */
    char *window_name = NULL;
    XFetchName(dpy, w, &window_name);
    draw_decorations(frame, window_name ? window_name : "Untitled");
    if (window_name) XFree(window_name);

    return frame;
}

/* Function to find client by frame or window */
Client *find_client(Window w) {
    Client *c = clients;
    while (c) {
        if (c->frame == w || c->window == w)
            return c;
        c = c->next;
    }
    return NULL;
}

/* Function to draw decorations on the frame */
void draw_decorations(Window frame, const char *title) {
    XClearWindow(dpy, frame);

    /* Draw title bar */
    XDrawString(dpy, frame, gc, 4, FONT_SIZE, title, strlen(title));

    /* Draw minimize, maximize, close buttons */
    int width = 50;
    int height = FONT_SIZE + 4;

    /* Close button */
    XDrawRectangle(dpy, frame, gc, 0, 0, width, height);
    XDrawString(dpy, frame, gc, 10, FONT_SIZE, "X", 1);

    /* Maximize button */
    XDrawRectangle(dpy, frame, gc, width, 0, width, height);
    XDrawString(dpy, frame, gc, width + 10, FONT_SIZE, "□", 1);

    /* Minimize button */
    XDrawRectangle(dpy, frame, gc, 2*width, 0, width, height);
    XDrawString(dpy, frame, gc, 2*width + 10, FONT_SIZE, "_", 1);
}

/* Main event loop */
int main(void)
{
    XWindowAttributes attr;
    XButtonEvent start;
    XEvent ev;

    /* Open display */
    if(!(dpy = XOpenDisplay(0x0))) return 1;
    root = DefaultRootWindow(dpy);

    /* Load font */
    font = XLoadQueryFont(dpy, FONT_NAME);
    if (!font) {
        fprintf(stderr, "Unable to load font %s\n", FONT_NAME);
        return 1;
    }

    /* Create graphics context */
    gc = XCreateGC(dpy, root, 0, NULL);
    XSetFont(dpy, gc, font->fid);
    XSetForeground(dpy, gc, BlackPixel(dpy, DefaultScreen(dpy)));

    /* Change cursor */
    default_cursor = XCreateFontCursor(dpy, DEFAULT_CURSOR);
    XDefineCursor(dpy, root, default_cursor);

    /* Grab keys */
    KeyCode left = XKeysymToKeycode(dpy, XK_Left);
    KeyCode right = XKeysymToKeycode(dpy, XK_Right);
    KeyCode up = XKeysymToKeycode(dpy, XK_Up);
    KeyCode down = XKeysymToKeycode(dpy, XK_Down);

    XGrabKey(dpy, left, MODIFIER, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, right, MODIFIER, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, up, MODIFIER, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy, down, MODIFIER, root, True, GrabModeAsync, GrabModeAsync);

    /* Grab buttons for moving and resizing */
    XGrabButton(dpy, 1, Mod1Mask, root, True,
                ButtonPressMask|ButtonReleaseMask|PointerMotionMask, GrabModeAsync, GrabModeAsync, None, None);
    XGrabButton(dpy, 3, Mod1Mask, root, True,
                ButtonPressMask|ButtonReleaseMask|PointerMotionMask, GrabModeAsync, GrabModeAsync, None, None);

    /* Select events */
    XSelectInput(dpy, root, SubstructureRedirectMask | SubstructureNotifyMask);

    start.subwindow = None;
    for(;;)
    {
        XNextEvent(dpy, &ev);

        if(ev.type == MapRequest)
        {
            /* New window wants to map */
            Window w = ev.xmaprequest.window;

            /* Create frame */
            Window frame = create_frame(w);

            /* Map the client window */
            XMapWindow(dpy, w);
        }
        else if(ev.type == ConfigureRequest)
        {
            /* Window wants to change its properties */
            XWindowChanges changes;
            changes.x = ev.xconfigurerequest.x;
            changes.y = ev.xconfigurerequest.y;
            changes.width = ev.xconfigurerequest.width;
            changes.height = ev.xconfigurerequest.height;
            changes.border_width = ev.xconfigurerequest.border_width;
            changes.sibling = ev.xconfigurerequest.above;
            changes.stack_mode = ev.xconfigurerequest.detail;

            Client *c = find_client(ev.xconfigurerequest.window);
            if (c) {
                /* Apply changes to the frame window */
                XConfigureWindow(dpy, c->frame, ev.xconfigurerequest.value_mask, &changes);
                /* Resize the client window */
                XResizeWindow(dpy, c->window, changes.width, changes.height - FONT_SIZE - 4);
            } else {
                XConfigureWindow(dpy, ev.xconfigurerequest.window, ev.xconfigurerequest.value_mask, &changes);
            }
        }
        else if(ev.type == Expose)
        {
            /* Redraw decorations */
            Client *c = find_client(ev.xexpose.window);
            if (c) {
                char *window_name = NULL;
                XFetchName(dpy, c->window, &window_name);
                draw_decorations(c->frame, window_name ? window_name : "Untitled");
                if (window_name) XFree(window_name);
            }
        }
        else if(ev.type == ButtonPress)
        {
            Client *c = find_client(ev.xbutton.window);
            if (!c) continue;

            start = ev.xbutton;

            /* Determine if click was on buttons */
            int x = ev.xbutton.x;
            int y = ev.xbutton.y;

            int button_width = 50;
            int button_height = FONT_SIZE + 4;

            if (y < button_height) {
                if (x < button_width) {
                    /* Close button */
                    XDestroyWindow(dpy, c->window);
                }
                else if (x < 2*button_width) {
                    /* Maximize button */
                    /* Implement maximize functionality */
                }
                else if (x < 3*button_width) {
                    /* Minimize button */
                    XUnmapWindow(dpy, c->frame);
                }
            }
            else {
                /* Start move or resize */
                XGetWindowAttributes(dpy, c->frame, &attr);

                int border_threshold = 5;
                if (x < border_threshold || x > attr.width - border_threshold ||
                    y < border_threshold || y > attr.height - border_threshold) {
                    /* Start resizing */
                    start.subwindow = c->frame;
                    start.button = 3; /* Use button 3 to indicate resizing */
                } else {
                    /* Start moving */
                    start.subwindow = c->frame;
                    start.button = 1; /* Use button 1 to indicate moving */
                }
            }
        }
        else if(ev.type == MotionNotify && start.subwindow != None)
        {
            int xdiff = ev.xbutton.x_root - start.x_root;
            int ydiff = ev.xbutton.y_root - start.y_root;
            Client *c = find_client(start.subwindow);
            if (!c) continue;

            XGetWindowAttributes(dpy, c->frame, &attr);

            if (start.button == 1)
            {
                /* Move window */
                XMoveWindow(dpy, c->frame,
                    attr.x + xdiff, attr.y + ydiff);
            }
            else if (start.button == 3)
            {
                /* Resize window */
                int new_width = MAX(1, attr.width + xdiff);
                int new_height = MAX(1, attr.height + ydiff);
                XResizeWindow(dpy, c->frame, new_width, new_height);
                XResizeWindow(dpy, c->window, new_width, new_height - FONT_SIZE - 4);
            }
        }
        else if(ev.type == ButtonRelease)
        {
            start.subwindow = None;
        }
        else if(ev.type == KeyPress)
        {
            /* Handle key bindings */
            Client *c = find_client(ev.xkey.subwindow);
            if (!c) continue;

            XGetWindowAttributes(dpy, c->frame, &attr);
            if (ev.xkey.keycode == left)
            {
                int new_width = MAX(1, attr.width - RESIZE_INCREMENT);
                XResizeWindow(dpy, c->frame, new_width, attr.height);
                XResizeWindow(dpy, c->window, new_width, attr.height - FONT_SIZE - 4);
            }
            else if (ev.xkey.keycode == right)
            {
                int new_width = attr.width + RESIZE_INCREMENT;
                XResizeWindow(dpy, c->frame, new_width, attr.height);
                XResizeWindow(dpy, c->window, new_width, attr.height - FONT_SIZE - 4);
            }
            else if (ev.xkey.keycode == up)
            {
                int new_height = MAX(1, attr.height - RESIZE_INCREMENT);
                XResizeWindow(dpy, c->frame, attr.width, new_height);
                XResizeWindow(dpy, c->window, attr.width, new_height - FONT_SIZE - 4);
            }
            else if (ev.xkey.keycode == down)
            {
                int new_height = attr.height + RESIZE_INCREMENT;
                XResizeWindow(dpy, c->frame, attr.width, new_height);
                XResizeWindow(dpy, c->window, attr.width, new_height - FONT_SIZE - 4);
            }
        }
        else if(ev.type == DestroyNotify)
        {
            /* Clean up */
            Client **cc = &clients;
            while (*cc) {
                if ((*cc)->window == ev.xdestroywindow.window) {
                    Client *tmp = *cc;
                    XDestroyWindow(dpy, tmp->frame);
                    *cc = tmp->next;
                    free(tmp);
                    break;
                }
                cc = &(*cc)->next;
            }
        }
    }
}
