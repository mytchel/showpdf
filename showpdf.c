#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <poppler.h>
#include <gtk/gtk.h>
#include <cairo.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>

#define MODE_HEIGHT 1
#define MODE_WIDTH  2
#define MODE_FIT    3

#define BACKGROUND_R 1
#define BACKGROUND_G 1
#define BACKGROUND_B 1

// Number of steps in a page.
#define STEPS 15

#define PAGES_FILE "/home/mytchel/.config/showpdf"
#define TMP_PAGES_FILE "/tmp/pages_file"

PopplerDocument *doc;
PopplerPage *page, *prev, *next;
int pages, current, yoffset, oldcurrent = -1;
gchar *file_name;
double page_width, page_height;
int mode = MODE_HEIGHT;

void get_current_page() {
    char line[4096];
    char *name, *page_str;
    FILE *page_file;
    
    current = 0;

    page_file = fopen(PAGES_FILE, "r");
    if (!page_file) {
        printf("No saved page file!!!\n");
        return;
    }

    while ((fgets(line, sizeof(char) * 4096, page_file)) != NULL) {
        name = strtok(line, ":");
        page_str = strtok(NULL, ":");

        if (strcmp(name, file_name) == 0) {
            current = atoi(page_str);
            break;
        }
    }

    fclose(page_file);
}

void save_current_page() {
    FILE *page_file, *tmp_file;
    char line[4096], *name;
    int page_num;

    page_file = fopen(PAGES_FILE, "r");
    tmp_file = fopen(TMP_PAGES_FILE, "w");
    if (!page_file || !tmp_file) {
        printf("ERROR opening files for saving current page\n");
        return;
    }

    while ((fgets(line, sizeof(char) * 4096, page_file)) != NULL) {
        name = strtok(line, ":");
        page_num = atoi(strtok(NULL, ":"));

        if (strcmp(name, file_name) == 0) {
            page_num = current;
            current = -1;
        }

        fprintf(tmp_file, "%s:%i\n", name, page_num);
    }

    if (current != -1)
        fprintf(tmp_file, "%s:%i\n", file_name, current);

    fclose(page_file);
    fclose(tmp_file);

    page_file = fopen(PAGES_FILE, "w");
    tmp_file = fopen(TMP_PAGES_FILE, "r");
    if (!page_file || !tmp_file) {
        printf("ERROR opening files for saving current page\n");
        return;
    }

    while ((fgets(line, sizeof(char) * 4096, tmp_file)) != NULL)
        fprintf(page_file, line);

    fclose(page_file);
    fclose(tmp_file);

    remove(TMP_PAGES_FILE);
}

void on_destroy(GtkWidget *w, gpointer data) {
    gtk_main_quit();
    save_current_page();
}

void render_pages(cairo_t *cr) {
   poppler_page_render(page, cr);

   if (yoffset < 0 && next) {
       cairo_translate(cr, 0, page_height);
       poppler_page_render(next, cr);
    } else if (prev) {
        cairo_translate(cr, 0, -page_height);
        poppler_page_render(prev, cr);
    }
}

gboolean on_expose(GtkWidget *w, GdkEventExpose *e, gpointer data) {
    double top, left, right, bottom;
    double scalex = 1, scaley = 1;
    int xoffset;
    
    gtk_widget_queue_draw(w);
    gint win_width, win_height;
    gtk_window_get_size(GTK_WINDOW(w), &win_width, &win_height);
   
    switch (mode) {
        case MODE_HEIGHT: scalex = scaley = win_height / page_height; break;
        case MODE_WIDTH:  scalex = scaley  = win_width  / page_width;  break;
        case MODE_FIT:
            scalex = win_width / page_width;
            scaley = win_height / page_height;
            break;
    }
    
    cairo_t *cr = gdk_cairo_create(w->window);

    // clear window. This is needed for some pdf's, probably just bad ones that I find.
    top = left = 0;
    right = win_width;
    bottom = win_height;
    cairo_set_source_rgb(cr, BACKGROUND_R, BACKGROUND_G, BACKGROUND_B);
    cairo_fill_extents(cr, &top, &left, &right, &bottom);
    cairo_fill(cr);
    cairo_paint(cr);

    xoffset = 0;
    if (page_width * scalex < win_width)
        xoffset = win_width / 2 - page_width * scalex / 2;

    cairo_translate(cr, xoffset, yoffset * win_height / STEPS);
 
    cairo_scale(cr, scalex, scaley);

    render_pages(cr);
    
    cairo_destroy(cr);
    return FALSE;
}

gboolean on_keypress(GtkWidget *w, GdkEvent *e, gpointer data) {
    switch (e->key.keyval) {
        case GDK_k: yoffset += 1; break;
        case GDK_j: yoffset -= 1; break;
        case GDK_c: yoffset = 0; break;
        case GDK_Page_Up: current -= (current > 0) ? 1 : 0; break;
        case GDK_Page_Down: current += (current + 1 < pages) ? 1 : 0; break;
        case GDK_Home: current = 0; break;
        case GDK_End: current = pages - 1; break;
        case GDK_space: current += (current + 1 < pages) ? 1 : 0; break;
        case GDK_w: mode = MODE_WIDTH; break;
        case GDK_h: mode = MODE_HEIGHT; break;
        case GDK_f: mode = MODE_FIT; break;
        case GDK_q: on_destroy(NULL, NULL); break;
    }

    if (yoffset < 0) {
        yoffset = STEPS - 1;
        if (current < pages - 1)
            current++;
    } else if (yoffset > STEPS) {
        yoffset = 1;
        if (current > 0) 
            current--;
    }

    if (oldcurrent != current) {
        g_object_unref(page);
        page = poppler_document_get_page(doc, current);
        if (!page) {
            puts("Could not open page of document");
            g_object_unref(page);
            exit(EXIT_FAILURE);
        }

        if (prev)
            g_object_unref(prev);
        if (next)
            g_object_unref(next);
        prev = next = NULL;
        if (current > 0)
            prev = poppler_document_get_page(doc, current - 1);
        if (current < pages -1)
            next = poppler_document_get_page(doc, current + 1);

        poppler_page_get_size(page, &page_width, &page_height);

        oldcurrent = current;
    }

    on_expose(w, NULL, NULL);
    
    return FALSE;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("Usage: %s FILE\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    gtk_init(&argc, &argv);
    GError *err = NULL;
   
    gchar *filename = argv[1], *absolute;
    if (g_path_is_absolute(filename)) {
        absolute = g_strdup (filename);
    } else {
        gchar *dir = g_get_current_dir ();
        absolute = g_build_filename (dir, filename, (gchar *) 0);
        free (dir);
    }

    file_name = g_strdup(absolute);

    gchar *uri = g_filename_to_uri (absolute, NULL, &err);
    free (absolute);
    if (uri == NULL) {
        printf("poppler fail: %s\n", err->message);
        exit(EXIT_FAILURE);
    }
   
    doc = poppler_document_new_from_file(uri, NULL, &err);
    if (!doc) {
        puts(err->message);
        g_object_unref(doc);
        exit(EXIT_FAILURE);
    }
    
    get_current_page();
   
    page = poppler_document_get_page(doc, current);
    if (!page) {
        puts("Could not open first page of document");
        g_object_unref(page);
        exit(EXIT_FAILURE);
    }

    poppler_page_get_size(page, &page_width, &page_height);
    pages = poppler_document_get_n_pages(doc);

    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    g_signal_connect(G_OBJECT(win), "destroy", G_CALLBACK(on_destroy), NULL);
    g_signal_connect(G_OBJECT(win), "expose-event", G_CALLBACK(on_expose), NULL);
    g_signal_connect(G_OBJECT(win), "key-press-event", G_CALLBACK(on_keypress), NULL);
    gtk_widget_set_app_paintable(win, TRUE);
    gtk_widget_show_all(win);
    gtk_main();
    g_object_unref(page);
    g_object_unref(doc);
}
