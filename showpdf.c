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

#define PAGE_FILE ".config/showpdf"
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

  { GDK_h, step_h, -1},
  { GDK_l, step_h, +1},
  
  { GDK_J, page_m, +1},
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

char *page_save_file;
gchar *file_name;
PopplerDocument *doc;
PopplerPage **pages;

double page_width, page_height;
gint win_width, win_height;

int current, oldcurrent, npages;

double yoffset, xoffset;
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
  scale = win_width / page_width;
}

void page_m(int d) {
  if (d > 0 && current < npages - 1 - d)
    current += d;
  else if (d < 0 && current > d)
    current += d;
}

void quit() {
  on_destroy(NULL, NULL);
}

void start() {
  current = 0;
}

void end() {
  current = npages - 1;
}

void zoom(int direction) {
  scale += direction * 0.1;
}

void get_current_page() {
  char line[4096];
  char *name, *page_str;
  FILE *page_file;
  
  current = 0;
  
  page_file = fopen(page_save_file, "r");
  if (!page_file) {
    printf("No saved page file!!!\n");
    page_file = fopen(page_save_file, "w");
    if (!page_file)
      printf("Could not write to \"%s\"\n", page_save_file);
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
  
  page_file = fopen(page_save_file, "r");
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
  
  page_file = fopen(page_save_file, "w");
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
  int i;
  cairo_matrix_t startmatrix, scaledmatrix;
  cairo_t *cr;
   
  // This causes the expose event to be triggered repeatidly on slackware (but not arch) causing
  // a noticable lag. So I've removed it for now.
  //gtk_widget_queue_draw(w);
  gtk_window_get_size(GTK_WINDOW(w), &win_width, &win_height);
  poppler_page_get_size(pages[current], &page_width, &page_height);
  
  cr = gdk_cairo_create(w->window);
  cairo_get_matrix(cr, &startmatrix);
  
  // clear window. This is needed for some pdf's, probably just bad ones that I find.
  top = left = 0;
  right = win_width;
  bottom = win_height;
  cairo_set_source_rgb(cr, BACKGROUND_R, BACKGROUND_G, BACKGROUND_B);
  cairo_fill_extents(cr, &left, &top, &right, &bottom);
  cairo_fill(cr);
  cairo_paint(cr);

  cairo_scale(cr, scale, scale);
  cairo_translate(cr, xoffset * page_width / STEPS, yoffset * page_height / STEPS);
  cairo_get_matrix(cr, &scaledmatrix);
  
  poppler_page_render(pages[current], cr);

  double realy = yoffset * page_width / STEPS * scale;

  // Render pages before current that can be seen.
  i = 1;
  do {
    if (current - i < 0) break;
    cairo_translate(cr, 0, -page_height);
    poppler_page_render(pages[current - i], cr);
    i++;
  } while (realy - i * page_height * scale > 0);
  
  cairo_set_matrix(cr, &scaledmatrix);

  // Render pages after current that can be seen.
  i = 1;
  do {
    if (current + i > npages) break;
    cairo_translate(cr, 0, page_height);
    poppler_page_render(pages[current + i], cr);
    i++;
  } while (realy + i * page_height * scale < win_height);

  cairo_set_matrix(cr, &startmatrix);
  
  char message[512];
  sprintf(message, "Page %i of %i", current, npages);
  
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
    if (current < npages - 1)
      current++;
  } else if (yoffset >= STEPS) {
    yoffset = 0;
    if (current > 0) 
      current--;
  }

  on_expose(w, NULL, NULL);
  
  return FALSE;
}

void load() {
  int i;
  GError *err = NULL;
 
  gchar *uri = g_filename_to_uri (file_name, NULL, &err);
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

  npages = poppler_document_get_n_pages(doc);  

  pages = malloc(sizeof(PopplerPage*) * npages);
 
  for (i = 0; i < npages; i++) {
    pages[i] = poppler_document_get_page(doc, i);

    if (!pages[i]) {
      printf("Could not open %i'th page of document", i);
      g_object_unref(pages[i]);
      exit(EXIT_FAILURE);
    }
  }
  
  oldcurrent = -1;
  scale = 1;
  xoffset = 0;
  yoffset = 0;

  get_current_page();
}

void unload() {
  int i;

  for (i = 0; i < npages; i++)
    g_object_unref(pages[i]);
  
  g_object_unref(doc);
}

int main(int argc, char **argv) {
  if (argc != 2) {
    printf("Usage: %s FILE\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  page_save_file = malloc(sizeof(char) * 1000);
  sprintf(page_save_file, "%s/%s", getenv("HOME"), PAGE_FILE);
  
  gtk_init(&argc, &argv);

  gchar *filename = argv[1], *absolute;
  if (g_path_is_absolute(filename)) {
    absolute = g_strdup (filename);
  } else {
    gchar *dir = g_get_current_dir ();
    absolute = g_build_filename (dir, filename, (gchar *) 0);
    free (dir);
  }
  
  file_name = g_strdup(absolute);

  load();
  
  GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  g_signal_connect(G_OBJECT(win), "destroy", G_CALLBACK(on_destroy), NULL);
  g_signal_connect(G_OBJECT(win), "expose-event", G_CALLBACK(on_expose), NULL);
  g_signal_connect(G_OBJECT(win), "key-press-event", G_CALLBACK(on_keypress), NULL);
  gtk_widget_set_app_paintable(win, TRUE);
  gtk_widget_show_all(win);
  gtk_main();

  unload();
  
  return 0;
}
