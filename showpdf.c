#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <poppler.h>
#include <gtk/gtk.h>
#include <cairo.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>

#define LEN(X) (sizeof(X)/sizeof(*X))

#define BACKGROUND_R 1
#define BACKGROUND_G 1
#define BACKGROUND_B 1

// Number of steps in a page.
#define STEPS 15

#define PAGES_FILE "/home/dbs/.config/showpdf"
#define TMP_PAGES_FILE "/tmp/pages_file"

typedef struct key key;
struct key {
  int key;
  void (*function)(int a);
  int arg;
};

void step_v(int d);
void step_h(int d);
void page_m(int d);
void quit();
void center();
void start();
void end();
void zoom(int ad);

void get_current_page();
void save_current_page();
void on_destroy(GtkWidget *w, gpointer data);
gboolean on_expose(GtkWidget *w, GdkEventExpose *e, gpointer data);
gboolean on_keypress(GtkWidget *w, GdkEvent *e, gpointer data);

struct key keys[] = {
  { GDK_j, step_v, -1},
  { GDK_k, step_v, +1},

  { GDK_h, step_h, +1},
  { GDK_l, step_h, -1},
  
  { GDK_J, page_m, 1},
  { GDK_K, page_m, -1},
  { GDK_Page_Down, page_m, 1},
  { GDK_Page_Up, page_m, -1},

  { GDK_plus, zoom, 1},
  { GDK_minus, zoom, -1},
  
  { GDK_c, center, 0},
  { GDK_Home, start, 0},
  { GDK_End, end, 0},
  
  { GDK_q, quit},
};

PopplerDocument *doc;
PopplerPage *page, *prev, *next;
gint win_width, win_height;
int pages, current, oldcurrent = -1;
int yoffset, xoffset;
gchar *file_name;
double page_width, page_height;

double scale;

void step_v(int d) {
  yoffset += d;
}

void step_h(int d) {
  xoffset += d;
}

// Not actually centering but fuck it.
void center() {
  xoffset = 0;
  yoffset = 0;
}

void page_m(int d) {
  if (d > 0 && current < pages - 1 - d)
    current += d;
  else if (d < 0 && current > d)
    current -= d;
}
void quit() {
  on_destroy(NULL, NULL);
}

void start() {
  current = 0;
}

void end() {
  current = pages - 1;
}

void zoom(int direction) {
  scale += direction * 0.1;
}

void get_current_page() {
  char line[4096];
  char *name, *page_str;
  FILE *page_file;
  
  current = 0;
  
  page_file = fopen(PAGES_FILE, "r");
  if (!page_file) {
    printf("No saved page file!!!\n");
    page_file = fopen(PAGES_FILE, "w");
    if (!page_file)
      printf("Could not write to \"%s\"\n", PAGES_FILE);
    else
      fclose(page_file);
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
    fprintf(page_file, "%s", line);
  
  fclose(page_file);
  fclose(tmp_file);
  
  remove(TMP_PAGES_FILE);
}

void on_destroy(GtkWidget *w, gpointer data) {
  gtk_main_quit();
  save_current_page();
}

gboolean on_expose(GtkWidget *w, GdkEventExpose *e, gpointer data) {
  double left, right, top, bottom;
   
  // This causes the expose event to be triggered repeatidly on slackware (but not arch) causing
  // a noticable lag. So I've removed it for now.
  //gtk_widget_queue_draw(w);
  gtk_window_get_size(GTK_WINDOW(w), &win_width, &win_height);
    
  cairo_t *cr = gdk_cairo_create(w->window);
  cairo_matrix_t matrix;
  cairo_get_matrix(cr, &matrix);
  
  // clear window. This is needed for some pdf's, probably just bad ones that I find.
  top = left = 0;
  right = win_width;
  bottom = win_height;
  cairo_set_source_rgb(cr, BACKGROUND_R, BACKGROUND_G, BACKGROUND_B);
  cairo_fill_extents(cr, &left, &top, &right, &bottom);
  cairo_fill(cr);
  cairo_paint(cr);
 
  cairo_translate(cr, (double) xoffset * win_width / STEPS, (double) yoffset * win_height / STEPS);
  double scalex = (win_width / page_width) * scale;
  cairo_scale(cr, scalex, scalex);
  
  poppler_page_render(page, cr);
  
  if (yoffset < 0 && next) {
    cairo_translate(cr, 0, page_height);
    poppler_page_render(next, cr);
  } else if (prev) {
    cairo_translate(cr, 0, -page_height);
    poppler_page_render(prev, cr);
  }
  
  cairo_set_matrix(cr,&matrix);

  char message[512];
  sprintf(message, "Page %i of %i", current, pages);
  
  cairo_move_to(cr, 10, win_height - 10);
  cairo_set_source_rgb(cr, 0, 0, 0);
  cairo_show_text(cr, message);
  
  cairo_destroy(cr);
  return FALSE;
}

gboolean on_keypress(GtkWidget *w, GdkEvent *e, gpointer data) {
  int i;
  int q = 0;
  for (i = 0; i < LEN(keys); i++) {
    if (keys[i].key == e->key.keyval) {
      keys[i].function(keys[i].arg);
      if (keys[i].function == quit) q = 1;
      break;
    }
  }
  if (q) return FALSE;

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

  scale = 1;
  xoffset = 0;
  yoffset = 0;
  
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
  
  return 0;
}
