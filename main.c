#include <stdio.h>
#include <gtk/gtk.h>
#include <png.h>
#include <stdlib.h>
#include <cairo/cairo.h>
#include <math.h>

GtkApplication *app;

struct widgets {
	GtkWidget *window;
	GtkWidget *box;
	GtkWidget *notebook;
	GtkWidget *header;
	GtkWidget *window_ap;
	GtkWidget *entry_ap;
	GtkWidget *button_ap;
	GtkWidget *entry_width_ap;
	GtkWidget *entry_height_ap;
	GtkWidget *button_add_ap;
	GtkWidget *clear_button;
	GtkWidget *cursor_button;

	GtkWidget *STUBS;
} w;

struct image {
	double width;
	double height;
	double x;
	double y;
	unsigned char *bytes;
	int pos;
};

struct info_tile {
	double x;
	double y;
	double width;
	double height;
};

struct eraser {
	double x;
	double y;
	double width;
	double height;
};

struct scene {
	int available;
	double x;
	double y;
	int pic;
	struct scene *next;
};

#define SCENE_LAYER           10

struct level {
	int width;
	int height;
	int tile_width;
	int tile_height;
	char filename[255];
};

#define STATE_CURSOR_BUTTON     0
#define STATE_CLEAR_BUTTON      1

struct list_files {
	int id;
	char *filename;
	char *name;
	int state_cursor;
	struct eraser eraser;
	GtkWidget *drawing;
	GtkWidget *frame;
	GtkWidget *label;
	GtkWidget *button_close;
	GtkWidget *frame_pics;
	GtkWidget *scroll_frame_pics;
	GtkWidget *draw_pics;
	GtkWidget *paned;
	GtkWidget *framen;
	GtkWidget *label_name;
	GtkWidget *box_label;
	GtkWidget *box_frame;
	GtkWidget *label_layer;
	GtkWidget *spin_layer;
	GtkWidget *box_layer;
	double pos_x;
	double pos_y;
	int grid;
	double offset_x;
	double offset_y;
	double size_width;
	double size_height;
	char *pic_path;
	int size_pics;
	struct image **im;
	cairo_surface_t **sur;
	struct info_tile *info;
	struct scene scene[SCENE_LAYER];
	int current_pic;
	struct list_files *next;
} *lf;


struct list_files *lm;

static void static_init_list_files ( ) {
	lf = calloc ( 1, sizeof ( struct list_files ) );
}

static char *get_dialog_file_name ( const char *title, const char *apply, GtkFileChooserAction action ) {
	GtkWidget *dialog;
	GtkFileChooser *chooser;
	int res;

	dialog = gtk_file_chooser_dialog_new ( title,
			( GtkWindow * ) w.window,
			action,
			"Отмена",
			GTK_RESPONSE_CANCEL,
			apply,
			GTK_RESPONSE_ACCEPT,
			NULL
			);

	chooser = GTK_FILE_CHOOSER ( dialog );

	gtk_file_chooser_set_do_overwrite_confirmation ( chooser, TRUE );

	res = gtk_dialog_run ( GTK_DIALOG ( dialog ) );

	if ( res == GTK_RESPONSE_ACCEPT ) {
		char *filename;
		filename = gtk_file_chooser_get_filename ( chooser );
		gtk_widget_destroy ( dialog );
		return filename;
	}

	gtk_widget_destroy ( dialog );
	return NULL;
}

static gboolean draw_pics_draw_cb ( GtkWidget *widget, cairo_t *cr, gpointer data ) {
	cairo_set_source_rgb ( cr, 0.2, 0.2, 0.2 );
	cairo_paint ( cr );

	double width, height;
	width = gtk_widget_get_allocated_width ( widget );
	height = gtk_widget_get_allocated_height ( widget );

	double x = 0;
	double y = 0;
	for ( int i = 0; i < lm->size_pics; i++ ) {
		if ( x + lm->size_width >= width ) {
			y += lm->size_height;
			x = 0;
		}
		lm->info[i].x = x;
		lm->info[i].y = y;
		cairo_set_source_surface ( cr, lm->sur[i], x, y );
		cairo_paint ( cr );

		x += lm->size_width;
	}


	return FALSE;
}
static gboolean draw_cb ( GtkWidget *widget, cairo_t *cr, gpointer data ) {

	struct list_files *l = ( struct list_files * ) data;

	cairo_set_source_rgb ( cr, 0.2, 0.2, 0.2 );
	cairo_paint ( cr );

	double width, height;
	width = gtk_widget_get_allocated_width ( widget );
	height = gtk_widget_get_allocated_height ( widget );

	cairo_set_source_rgb ( cr, 1.0, 1.0, 1.0 );

	double x, y;

	if ( l->grid ) {
		for ( x = l->pos_x, y = l->pos_y; y < height; y += l->size_height ) {
			cairo_move_to ( cr, x, y );
			cairo_line_to ( cr, width, y );
		}
		for ( x = l->pos_x, y = l->pos_y; x < width; x += l->size_width ) {
			cairo_move_to ( cr, x, y );
			cairo_line_to ( cr, x, height );
		}
		cairo_stroke ( cr );
	}

	if ( l->current_pic >= 0 ) {
		cairo_set_source_surface ( cr, l->sur[l->current_pic], l->info[l->current_pic].x, l->info[l->current_pic].y );
		cairo_paint ( cr );
	}

	for ( int i = 0; i < SCENE_LAYER; i++ ) {
		struct scene *sc = &lm->scene[i];
		while ( sc->next ) {
			if ( sc->available ) {
				cairo_set_source_surface ( cr, l->sur[sc->pic], sc->x, sc->y );
				cairo_paint ( cr );
			}

			if ( !sc->next ) break;
			sc = sc->next;
		}
	}

	return FALSE;
}

static int btn_m;
static double px;
static double py;
static int btn_sel;
static int paint_bool;

static gboolean draw_button_motion_event_cb ( GtkWidget *widget, GdkEvent *event, gpointer data ) {
	GdkEventMotion *evm = ( GdkEventMotion * ) event;
	struct list_files *l = ( struct list_files * ) data;

	if ( btn_m ) {
		double x = evm->x;
		double y = evm->y;
		double temp_x = 0.0;
		double temp_y = 0.0;

		if ( l->pos_x > 0 ) {
			l->pos_x -= x - px + l->size_width;
		} else {
			double size = -l->size_width;
			if ( l->pos_x <= -l->size_width ) {
				l->pos_x += l->size_width;
				temp_x = x - px;
			}
			else l->pos_x -= x - px;
		}
		if ( l->pos_y > 0 ) {
			l->pos_y -= y - py + l->size_height;
		} else {
			if ( l->pos_y < -l->size_height ) {
				l->pos_y += l->size_height;
				temp_y = y - py;
			}
			else l->pos_y -= y - py;
		}

		for ( int i = 0; i < SCENE_LAYER; i++ ) {
			struct scene *sc = &lm->scene[i];

			while ( sc->next ) {

				if ( sc->available ) {
					double xx = x - px;
					double yy = y - py;

					if ( xx < 0.0 ) sc->x += fabs ( xx );
					else if ( xx > 0.0 ) sc->x -= fabs ( xx ) - temp_x;

					if ( yy < 0.0 ) sc->y += fabs ( yy );
					else if ( yy > 0.0 ) sc->y -= fabs ( yy ) - temp_y;
				}

				if ( !sc->next ) break;

				sc = sc->next;
			}
		}


		px = x;
		py = y;

	}


	if ( paint_bool ) {
		int layer = gtk_spin_button_get_value ( ( GtkSpinButton * ) lm->spin_layer );
		struct scene *sc = &lm->scene[layer];
		double x = lm->info[lm->current_pic].x;
		double y = lm->info[lm->current_pic].y;
		double erx = lm->eraser.x;
		double ery = lm->eraser.y;
		int found = 0;
		while ( sc->next ) {
			if ( sc->available ) {
				if ( lm->state_cursor == STATE_CURSOR_BUTTON ) {
					if ( sc->x == x && sc->y == y ) {
						sc->pic = lm->current_pic;
						found = 1;
						break;
					}
				} else if ( lm->state_cursor == STATE_CLEAR_BUTTON ) {
					if ( sc->x == erx && sc->y == ery ) {
						sc->available = 0;
						found = 1;
						break;
					}
				}
			}

			if ( !sc->next ) break;
			sc = sc->next;
		}
		if ( !found ) {
			if ( lm->state_cursor == STATE_CURSOR_BUTTON ) {
				if ( !sc->available ) {
					sc->available = 1;
					sc->x = x;
					sc->y = y;
					sc->pic = lm->current_pic;
				} else {
					sc->next = calloc ( 1, sizeof ( struct scene ) );
					sc = sc->next;
					sc->available = 1;
					sc->x = x;
					sc->y = y;
					sc->pic = lm->current_pic;
				}
			}
		}
	}

	if ( ( lm->current_pic >= 0 ) || ( lm->state_cursor == STATE_CLEAR_BUTTON ) ) {
		double width, height;
		width = gtk_widget_get_allocated_width ( widget );
		height = gtk_widget_get_allocated_height ( widget );
		int found = 0;
		for ( double y = l->pos_y; y < height; y += l->size_height ) {
			for ( double x = l->pos_x; x < width; x += l->size_width ) {
				if ( evm->x >= x && evm->x <= x + l->size_width ) {
					if ( evm->y >= y && evm->y <= y + l->size_height ) {
						lm->info[l->current_pic].x = x;
						lm->info[l->current_pic].y = y;
						lm->eraser.x = x;
						lm->eraser.y = y;
						found = 1;
						break;
					}	
				}
			}
			if ( found ) break;
		}

	}

	gtk_widget_queue_draw ( l->drawing );

	return FALSE;
}

static gboolean draw_button_press_event_cb ( GtkWidget *widget, GdkEvent *event, gpointer data ) {
	GdkEventButton *evb = ( GdkEventButton * ) event;

	if ( evb->type == GDK_BUTTON_PRESS && evb->button == 2 ) {
		btn_m = 1;
		px = evb->x;
		py = evb->y;

		return FALSE;
	}

	if ( evb->type == GDK_BUTTON_PRESS && evb->button == 1 && ( ( lm->current_pic >= 0 ) || ( lm->state_cursor == STATE_CLEAR_BUTTON ) ) ) {
		paint_bool = 1;
		int layer = gtk_spin_button_get_value ( ( GtkSpinButton * ) lm->spin_layer );
		struct scene *sc = &lm->scene[layer];
		double x = lm->info[lm->current_pic].x;
		double y = lm->info[lm->current_pic].y;
		double erx = lm->eraser.x;
		double ery = lm->eraser.y;
		int found = 0;
		while ( sc->next ) {
			if ( sc->available ) {
				if ( lm->state_cursor == STATE_CURSOR_BUTTON ) {
					if ( sc->x == x && sc->y == y ) {
						sc->pic = lm->current_pic;
						found = 1;
						break;
					}
				} else if ( lm->state_cursor == STATE_CLEAR_BUTTON ) {
					if ( sc->x == erx && sc->y == ery ) {
						sc->available = 0;
						found = 1;
						break;
					}
				}
			}
			if ( found ) break;

			if ( !sc->next ) break;
			sc = sc->next;
		}
		if ( !found ) {
			if ( lm->state_cursor == STATE_CURSOR_BUTTON ) {
				if ( !sc->available ) {
					sc->available = 1;
					sc->x = x;
					sc->y = y;
					sc->pic = lm->current_pic;
				} else {
					sc->next = calloc ( 1, sizeof ( struct scene ) );
					sc = sc->next;
					sc->available = 1;
					sc->x = x;
					sc->y = y;
					sc->pic = lm->current_pic;
				}
			}
		}
	}

	return FALSE;
}
static gboolean draw_button_release_event_cb ( GtkWidget *widget, GdkEvent *event, gpointer data ) {
	GdkEventButton *evb = ( GdkEventButton * ) event;

	if ( evb->type == GDK_BUTTON_RELEASE ) {
		btn_m = 0;
		paint_bool = 0;
	}

	return FALSE;
}

static gboolean draw_pics_button_press_event_cb ( GtkWidget *widget, GdkEvent *event, gpointer data ) {
	struct list_files *l = ( struct list_files * ) data;
	GdkEventButton *evb = ( GdkEventButton * ) event;

	if ( evb->type == GDK_BUTTON_PRESS && evb->button == 1 ) {
		btn_sel = 1;
		int found = 0;
		for ( int i = 0; i < l->size_pics; i++ ) {
			if ( l->info ) {
				if ( evb->x >= l->info[i].x && evb->x <= l->info[i].x + l->info[i].width ) {
					if ( evb->y >= l->info[i].y && evb->y <= l->info[i].y + l->info[i].height ) {
						l->current_pic = i;
						found = 1;
						break;
					}
				}
			}
		}
		if ( !found ) l->current_pic = -1;
	}
	return FALSE;
}
static gboolean draw_pics_button_release_event_cb ( GtkWidget *widget, GdkEvent *event, gpointer data ) {
	GdkEventButton *evb = ( GdkEventButton * ) event;

	if ( evb->type == GDK_BUTTON_RELEASE ) {
	}

	return FALSE;
}

static void notebook_switch_page_cb ( GtkNotebook *notebook, GtkWidget *page, int page_num, gpointer data ) {
	struct list_files *l = lf;
	while ( l->next ) {
		if ( l->id == page_num ) {
			lm = l;
			return;
		}
		l = l->next;
	}
}

static void activate_new_project ( GSimpleAction *simple, GVariant *parameter, gpointer data ) {
	struct list_files *l = lf;
	while ( l->next ) l = l->next;
	l->current_pic = -1;

	l->filename = get_dialog_file_name ( "Новый проект", "Создать", GTK_FILE_CHOOSER_ACTION_SAVE );

	if ( !l->filename ) return;

	struct level level;
	level.width = 0;
	level.height = 0;
	FILE *fp = fopen ( l->filename, "w" );
	fwrite ( &level, 1, sizeof ( struct level ), fp );
	fclose ( fp );

	l->state_cursor = STATE_CURSOR_BUTTON;
	gtk_toggle_button_set_active ( ( GtkToggleButton * ) w.cursor_button, TRUE );
	gtk_toggle_button_set_active ( ( GtkToggleButton * ) w.clear_button, FALSE );

	l->name = strrchr ( l->filename, '/' );
	l->name++;

	l->frame = g_object_new ( GTK_TYPE_FRAME, "shadow-type", GTK_SHADOW_NONE, NULL );
	GtkWidget *box = gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	GtkWidget *drawing = gtk_drawing_area_new ( );
	l->drawing = drawing;
	l->size_width = l->size_height = 64;
	g_signal_connect ( drawing, "draw", G_CALLBACK ( draw_cb ), l );
	g_signal_connect ( drawing, "button-press-event", G_CALLBACK ( draw_button_press_event_cb ), l );
	g_signal_connect ( drawing, "button-release-event", G_CALLBACK ( draw_button_release_event_cb ), l );
	g_signal_connect ( drawing, "motion-notify-event", G_CALLBACK ( draw_button_motion_event_cb ), l );
	gtk_widget_add_events ( drawing, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_BUTTON2_MOTION_MASK | GDK_POINTER_MOTION_MASK );
	GtkWidget *paned = gtk_paned_new ( GTK_ORIENTATION_HORIZONTAL );
	l->paned = paned;

	GtkWidget *frame = g_object_new ( GTK_TYPE_FRAME, "shadow-type", GTK_SHADOW_NONE, NULL );
	l->framen = frame;
	gtk_widget_set_size_request ( drawing, 1080, -1 );
	gtk_widget_set_size_request ( frame, 196, -1 );

	gtk_paned_pack1 ( ( GtkPaned * ) paned, drawing, FALSE, FALSE );
	gtk_paned_pack2 ( ( GtkPaned * ) paned, frame, FALSE, FALSE );

	gtk_box_pack_start ( ( GtkBox * ) box, paned, TRUE, TRUE, 0 );

	gtk_container_add ( ( GtkContainer * ) l->frame, box );

	GtkWidget *label = gtk_label_new ( l->name );
	l->label_name = label;
	GtkWidget *button = gtk_button_new_from_icon_name ( "exit" , GTK_ICON_SIZE_MENU );
	l->button_close = button;
	GtkWidget *box_label = gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	l->box_label = box_label;
	gtk_box_pack_start ( ( GtkBox * ) box_label, label, FALSE, FALSE, 0 );
	gtk_box_pack_end ( ( GtkBox * ) box_label, button, FALSE, FALSE, 0 );

	GtkWidget *box_frame = gtk_box_new ( GTK_ORIENTATION_VERTICAL, 0 );
	l->box_frame = box_frame;
	gtk_container_add ( ( GtkContainer * ) frame, box_frame );

	GtkWidget *label_layer = gtk_label_new ( "Слой" );
	l->label_layer = label_layer;
	gtk_widget_set_margin_top ( label_layer, 16 );
	gtk_widget_set_margin_start ( label_layer, 16 );
	gtk_widget_set_margin_end ( label_layer, 16 );

	GtkWidget *spin_layer = gtk_spin_button_new_with_range ( 0, 9, 1 );
	l->spin_layer = spin_layer;
	gtk_widget_set_margin_top ( spin_layer, 16 );
	gtk_widget_set_margin_start ( spin_layer, 16 );
	gtk_widget_set_margin_end ( spin_layer, 16 );

	GtkWidget *box_layer = gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	l->box_layer = box_layer;
	gtk_box_pack_start ( ( GtkBox * ) box_layer, label_layer, FALSE, FALSE, 0 );
	gtk_box_pack_end ( ( GtkBox * ) box_layer, spin_layer, FALSE, FALSE, 0 );

	l->frame_pics = g_object_new ( GTK_TYPE_FRAME, "shadow-type", GTK_SHADOW_NONE, NULL );
	l->draw_pics = gtk_drawing_area_new ( );
	g_signal_connect ( l->draw_pics, "draw", G_CALLBACK ( draw_pics_draw_cb ), l );
	g_signal_connect ( l->draw_pics, "button-press-event", G_CALLBACK ( draw_pics_button_press_event_cb ), l );
	g_signal_connect ( l->draw_pics, "button-release-event", G_CALLBACK ( draw_pics_button_release_event_cb ), l );
	gtk_widget_add_events ( l->draw_pics, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK );
	l->scroll_frame_pics = gtk_scrolled_window_new ( NULL, NULL );
	gtk_container_add ( ( GtkContainer * ) l->scroll_frame_pics, l->draw_pics );
	gtk_container_add ( ( GtkContainer * ) l->frame_pics, l->scroll_frame_pics );
	gtk_widget_set_margin_top ( l->frame_pics, 16 );
	gtk_widget_set_margin_start ( l->frame_pics, 16 );
	gtk_widget_set_margin_end ( l->frame_pics, 16 );
	gtk_widget_set_margin_bottom ( l->frame_pics, 16 );

	gtk_box_pack_start ( ( GtkBox * ) box_frame, box_layer, FALSE, FALSE, 0 );
	gtk_box_pack_start ( ( GtkBox * ) box_frame, l->frame_pics, TRUE, TRUE, 0 );

	gtk_notebook_append_page ( ( GtkNotebook * ) w.notebook, l->frame, box_label );
	l->id = gtk_notebook_page_num ( ( GtkNotebook * ) w.notebook, l->frame );

	g_signal_connect ( w.notebook, "switch-page", G_CALLBACK ( notebook_switch_page_cb ), NULL );

	lm = l;

	gtk_widget_show_all ( l->frame );
	gtk_widget_show_all ( box_label );
	gtk_widget_show_all ( w.notebook );

	l->next = calloc ( 1, sizeof ( struct list_files ) );

}
static void write_to_file ( struct scene *sc, int sx, int sy, int width, int height ) {
	int map[height][width];
	for ( int y = 0; y < height; y++ ) {
		for ( int x = 0; x < width; x++ ) {
			map[y][x] = -1;
		}
	}

	while ( sc->next ) {
		//printf ( "%f %d %d\n", sc->x, sx, eex );
		if ( sc->available ) {
			int x = sc->x - ( sx < 0 ? sx - 1 : sx );
			int y = sc->y - ( sy < 0 ? sy - 1 : sy );
			x /= lm->size_width;
			y /= lm->size_height;

			map[y][x] = sc->pic;
		}

		if ( !sc->next ) break;

		sc = sc->next;
	}

	FILE *fp = fopen ( lm->filename, "a+" );
	fwrite ( map, 1, sizeof ( map ), fp );
	fclose ( fp );
}
static void activate_save_project ( GSimpleAction *simple, GVariant *parameter, gpointer data ) {
	struct level level;
	int sx, sy, ex, ey;
	int ff = 0;
	int temp_x, temp_y;
	for ( int i = 0; i < SCENE_LAYER; i++ ) {
		struct scene *sc = &lm->scene[i];
		while ( sc->next ) {

			if ( sc->available ) {
				temp_x = sc->x;
				temp_y = sc->y;
				if ( !ff ) {
					sx = ex = temp_x;
					sy = ey = temp_y;
					ff = 1;
				} else {
					if ( temp_x < sx ) sx = temp_x;
					if ( temp_y < sy ) sy = temp_y;
					if ( temp_x > ex ) ex = temp_x;
					if ( temp_y > ey ) ey = temp_y;
				}
			}

			if ( !sc->next ) break;

			sc = sc->next;
		}
	}

	int eex = ex;
	int eey = ey;
	ex -= sx < 0 ? sx - 1 : sx;
	ey -= sy < 0 ? sy - 1 : sy;
	int xx = sx;
	int yy = sy;
	ex /= lm->size_width;
	ey /= lm->size_height;

	ex++;
	ey++;

	level.width = ex;
	level.height = ey;
	level.tile_width = lm->size_width;
	level.tile_height = lm->size_height;
	strncpy ( level.filename, lm->pic_path, 255 );

	FILE *fp = fopen ( lm->filename, "w" );
	fwrite ( &level, 1, sizeof ( struct level ), fp );
	fclose ( fp );

	for ( int i = 0; i < SCENE_LAYER; i++ ) {
		struct scene *sc = &lm->scene[i];
		write_to_file ( sc, sx, sy, level.width, level.height );
	}
}
static void activate_save_as_project ( GSimpleAction *simple, GVariant *parameter, gpointer data ) {
}
static void activate_quit ( GSimpleAction *simple, GVariant *parameter, gpointer data ) {
	g_application_release ( ( GApplication * ) app );
}

static void activate_add_picture ( GSimpleAction *simple, GVariant *parameter, gpointer data ) {
	gtk_widget_show_all ( w.window_ap );
}

static void read_pic ( char *pic_path, int state_file );

static void activate_open_project ( GSimpleAction *simple, GVariant *parameter, gpointer data ) {
	struct list_files *l = lf;
	while ( l->next ) l = l->next;
	l->current_pic = -1;

	l->filename = get_dialog_file_name ( "Открыть проект", "Открыть", GTK_FILE_CHOOSER_ACTION_OPEN );

	if ( !l->filename ) return;

	l->name = strrchr ( l->filename, '/' );
	l->name++;

	l->state_cursor = STATE_CURSOR_BUTTON;

	gtk_toggle_button_set_active ( ( GtkToggleButton * ) w.cursor_button, TRUE );
	gtk_toggle_button_set_active ( ( GtkToggleButton * ) w.clear_button, FALSE );

	l->frame = g_object_new ( GTK_TYPE_FRAME, "shadow-type", GTK_SHADOW_NONE, NULL );
	GtkWidget *box = gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	GtkWidget *drawing = gtk_drawing_area_new ( );
	l->drawing = drawing;
	l->size_width = l->size_height = 64;
	g_signal_connect ( drawing, "draw", G_CALLBACK ( draw_cb ), l );
	g_signal_connect ( drawing, "button-press-event", G_CALLBACK ( draw_button_press_event_cb ), l );
	g_signal_connect ( drawing, "button-release-event", G_CALLBACK ( draw_button_release_event_cb ), l );
	g_signal_connect ( drawing, "motion-notify-event", G_CALLBACK ( draw_button_motion_event_cb ), l );
	gtk_widget_add_events ( drawing, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_BUTTON2_MOTION_MASK | GDK_POINTER_MOTION_MASK );
	GtkWidget *paned = gtk_paned_new ( GTK_ORIENTATION_HORIZONTAL );
	l->paned = paned;

	GtkWidget *frame = g_object_new ( GTK_TYPE_FRAME, "shadow-type", GTK_SHADOW_NONE, NULL );
	l->framen = frame;
	gtk_widget_set_size_request ( drawing, 1080, -1 );
	gtk_widget_set_size_request ( frame, 196, -1 );

	gtk_paned_pack1 ( ( GtkPaned * ) paned, drawing, FALSE, FALSE );
	gtk_paned_pack2 ( ( GtkPaned * ) paned, frame, FALSE, FALSE );

	gtk_box_pack_start ( ( GtkBox * ) box, paned, TRUE, TRUE, 0 );

	gtk_container_add ( ( GtkContainer * ) l->frame, box );

	GtkWidget *label = gtk_label_new ( l->name );
	l->label_name = label;
	GtkWidget *button = gtk_button_new_from_icon_name ( "exit" , GTK_ICON_SIZE_MENU );
	l->button_close = button;
	GtkWidget *box_label = gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	l->box_label = box_label;
	gtk_box_pack_start ( ( GtkBox * ) box_label, label, FALSE, FALSE, 0 );
	gtk_box_pack_end ( ( GtkBox * ) box_label, button, FALSE, FALSE, 0 );

	GtkWidget *box_frame = gtk_box_new ( GTK_ORIENTATION_VERTICAL, 0 );
	l->box_frame = box_frame;
	gtk_container_add ( ( GtkContainer * ) frame, box_frame );

	GtkWidget *label_layer = gtk_label_new ( "Слой" );
	l->label_layer = label_layer;
	gtk_widget_set_margin_top ( label_layer, 16 );
	gtk_widget_set_margin_start ( label_layer, 16 );
	gtk_widget_set_margin_end ( label_layer, 16 );

	GtkWidget *spin_layer = gtk_spin_button_new_with_range ( 0, 9, 1 );
	l->spin_layer = spin_layer;
	gtk_widget_set_margin_top ( spin_layer, 16 );
	gtk_widget_set_margin_start ( spin_layer, 16 );
	gtk_widget_set_margin_end ( spin_layer, 16 );

	GtkWidget *box_layer = gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	l->box_layer = box_layer;
	gtk_box_pack_start ( ( GtkBox * ) box_layer, label_layer, FALSE, FALSE, 0 );
	gtk_box_pack_end ( ( GtkBox * ) box_layer, spin_layer, FALSE, FALSE, 0 );

	l->frame_pics = g_object_new ( GTK_TYPE_FRAME, "shadow-type", GTK_SHADOW_NONE, NULL );
	l->draw_pics = gtk_drawing_area_new ( );
	g_signal_connect ( l->draw_pics, "draw", G_CALLBACK ( draw_pics_draw_cb ), l );
	g_signal_connect ( l->draw_pics, "button-press-event", G_CALLBACK ( draw_pics_button_press_event_cb ), l );
	g_signal_connect ( l->draw_pics, "button-release-event", G_CALLBACK ( draw_pics_button_release_event_cb ), l );
	gtk_widget_add_events ( l->draw_pics, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK );
	l->scroll_frame_pics = gtk_scrolled_window_new ( NULL, NULL );
	gtk_container_add ( ( GtkContainer * ) l->scroll_frame_pics, l->draw_pics );
	gtk_container_add ( ( GtkContainer * ) l->frame_pics, l->scroll_frame_pics );
	gtk_widget_set_margin_top ( l->frame_pics, 16 );
	gtk_widget_set_margin_start ( l->frame_pics, 16 );
	gtk_widget_set_margin_end ( l->frame_pics, 16 );
	gtk_widget_set_margin_bottom ( l->frame_pics, 16 );

	gtk_box_pack_start ( ( GtkBox * ) box_frame, box_layer, FALSE, FALSE, 0 );
	gtk_box_pack_start ( ( GtkBox * ) box_frame, l->frame_pics, TRUE, TRUE, 0 );

	gtk_notebook_append_page ( ( GtkNotebook * ) w.notebook, l->frame, box_label );
	l->id = gtk_notebook_page_num ( ( GtkNotebook * ) w.notebook, l->frame );

	g_signal_connect ( w.notebook, "switch-page", G_CALLBACK ( notebook_switch_page_cb ), NULL );

	lm = l;

	gtk_widget_show_all ( l->frame );
	gtk_widget_show_all ( box_label );
	gtk_widget_show_all ( w.notebook );

	l->next = calloc ( 1, sizeof ( struct list_files ) );

	struct level level;
	FILE *fp = fopen ( l->filename, "r" );
	fread ( &level, 1, sizeof ( level ), fp );

	l->size_width = level.tile_width;
	l->size_height = level.tile_height;

	/* часть чтения картинки */
	read_pic ( level.filename, 1 );
	l->pic_path = strdup ( level.filename );

	int map[level.height][level.width];

	for ( int i = 0; i < SCENE_LAYER; i++ ) {
		fread ( map, 1, sizeof ( map ), fp );

		struct scene *sc = &l->scene[i];

		for ( int y = 0; y < level.height; y++ ) {
			for ( int x = 0; x < level.width; x++ ) {
				if ( map[y][x] >= 0 ) {
					sc->x = lm->size_width * x;
					sc->y = lm->size_height * y;
					sc->pic = map[y][x];
					sc->available = 1;
					sc->next = calloc ( 1, sizeof ( struct scene ) );
					sc = sc->next;
				}
			}
		}

	}
	fclose ( fp );
}

static void read_pic ( char *pic_path, int state_file ) {
	int width, height;
	png_byte color_type;
	png_byte bit_depth;
	png_bytep *row = NULL;

	FILE *fp = fopen ( pic_path, "r" );
	png_structp png = png_create_read_struct ( PNG_LIBPNG_VER_STRING, NULL, NULL, NULL );
	if ( !png ) {
		fclose ( fp );
		return;
	}

	png_infop info = png_create_info_struct ( png );
	if ( !info ) {
		fclose ( fp );
		png_destroy_read_struct ( &png, NULL, NULL );
		return;
	}

	if ( setjmp ( png_jmpbuf ( png ) ) ) {
		fclose ( fp );
		png_destroy_read_struct ( &png, &info, NULL );
		return;
	}

	png_init_io ( png, fp );

	png_read_info ( png, info );

	width = png_get_image_width ( png, info );
	height = png_get_image_height ( png, info );
	color_type = png_get_color_type ( png, info );
	bit_depth = png_get_bit_depth ( png, info );

	if ( color_type != PNG_COLOR_TYPE_RGBA ) {
		fclose ( fp );
		png_destroy_read_struct ( &png, &info, NULL );
		return;
	}


	int ww = atoi ( gtk_entry_get_text ( ( GtkEntry * ) w.entry_width_ap ) );
	int hw = atoi ( gtk_entry_get_text ( ( GtkEntry * ) w.entry_height_ap ) );
	if ( state_file ) {
		ww = lm->size_width;
		hw = lm->size_height;
		lm->grid = 1;
	} else {
		ww = atoi ( gtk_entry_get_text ( ( GtkEntry * ) w.entry_width_ap ) );
		hw = atoi ( gtk_entry_get_text ( ( GtkEntry * ) w.entry_height_ap ) );
		lm->size_width = ww;
		lm->size_height = hw;
		lm->grid = 1;
	}

	int fill_width = width / ww;
	int fill_height = height / hw;

	int co = 0;

	row = calloc ( height, sizeof ( png_bytep ) );
	for ( int i = 0; i < height; i++ ) {
		int count = png_get_rowbytes ( png, info ) / 4 / ww;
		row[i] = calloc ( png_get_rowbytes ( png, info ), 1 );
		co += count;
	}
	co /= height;

	png_read_image ( png, row );
	fclose ( fp );
	png_destroy_read_struct ( &png, &info, NULL );
	if ( fill_width == 0 ) fill_width = 1;
	if ( fill_height == 0 ) fill_height = 1;

	int stride = cairo_format_stride_for_width ( CAIRO_FORMAT_ARGB32, width );

	lm->im = calloc ( fill_width * fill_height, sizeof ( struct image * ) );
	for ( int y = 0; y < fill_height; y++ ) {
		lm->im[y] = calloc ( width, sizeof ( struct image ) );
		for ( int x = 0; x < fill_width; x++ ) {
			lm->im[y][x].bytes = calloc ( ww * 4 * hw * stride, 1 );
		}
	}

	int i = 0;
	int c = 0;

	int yy = 0;
	int xx = 0;
	int oldyy = -1;
	int oldxx = -1;

	for ( int y = 0, x = 0; y < height; y++ ) {
		for ( int xs = 0, x = 0; x < width; x++, xs += 4, i += 4 ) {
			int pos = lm->im[y / hw][x / ww].pos;
			lm->im[y / hw][x / ww].pos += 4;
			lm->im[y / hw][x / ww].bytes[stride * y + pos + 0] = row[y][i + 2];
			lm->im[y / hw][x / ww].bytes[stride * y + pos + 1] = row[y][i + 1];
			lm->im[y / hw][x / ww].bytes[stride * y + pos + 2] = row[y][i + 0];
			lm->im[y / hw][x / ww].bytes[stride * y + pos + 3] = row[y][i + 3];
			yy = y / hw;
			xx = x / ww;
			if ( xx != oldxx ) {
				oldyy = yy;
				oldxx = xx;
				lm->im[oldyy][oldxx].pos = 0;
			}
		}
		lm->im[oldyy][oldxx].pos = 0;
		lm->im[yy][xx].pos = 0;
		i = 0;
	}

	lm->sur = calloc ( fill_width * fill_height, sizeof ( char * ) );
	lm->size_pics = fill_width * fill_height;
	lm->info = calloc ( fill_width * fill_height, sizeof ( struct info_tile ) );

	i = 0;
	for ( int y = 0; y < fill_height; y++ ) {
		for ( int x = 0; x < fill_width; x++ ) {
			lm->sur[i] = cairo_image_surface_create_for_data ( lm->im[y][x].bytes, CAIRO_FORMAT_ARGB32, ww, hw, stride );
			lm->info[i].width = ww;
			lm->info[i].height = hw;
			i++;
		}
	}

	gtk_widget_queue_draw ( lm->draw_pics );
}

static void button_add_ap_cb ( GtkButton *button, gpointer data ) {
	read_pic ( lm->pic_path, 0 );
}

static void button_ap_cb ( GtkButton *button, gpointer data ) {
	int index = gtk_notebook_get_current_page ( ( GtkNotebook * ) w.notebook );
	if ( index == -1 ) return;
	if ( lm->pic_path ) g_free ( lm->pic_path );

	lm->pic_path = get_dialog_file_name ( "Открыть картинку", "Открыть", GTK_FILE_CHOOSER_ACTION_OPEN );

	if ( !lm->pic_path ) return;

	gtk_entry_set_text ( ( GtkEntry * ) w.entry_ap, lm->pic_path );
}

static gboolean window_ap_delete_event_cb ( GtkWidget *widget, gpointer data ) {
	gtk_widget_hide ( w.window_ap );
	return TRUE;
}

static void static_init_add_picture ( ) {
	w.window_ap = gtk_application_window_new ( app );
	gtk_window_set_modal ( ( GtkWindow * ) w.window_ap, TRUE );
	gtk_window_set_transient_for ( ( GtkWindow * ) w.window_ap, ( GtkWindow * ) w.window );
	g_signal_connect ( w.window_ap, "delete-event", G_CALLBACK ( window_ap_delete_event_cb ), NULL );
	gtk_window_set_default_size ( ( GtkWindow * ) w.window_ap, 400, 200 );
	GtkWidget *box = gtk_box_new ( GTK_ORIENTATION_VERTICAL, 0 );
	gtk_container_add ( ( GtkContainer * ) w.window_ap, box );

	GtkWidget *box_path = gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	GtkWidget *label_path = gtk_label_new ( "Путь к файлу png" );
	w.entry_ap = gtk_entry_new ( );
	w.button_ap = gtk_button_new_with_label ( "Выбрать" );
	gtk_box_pack_start ( ( GtkBox * ) box_path, label_path, FALSE, FALSE, 0 );
	gtk_box_pack_end ( ( GtkBox * ) box_path, w.button_ap, FALSE, FALSE, 0 );
	gtk_box_pack_end ( ( GtkBox * ) box_path, w.entry_ap, FALSE, FALSE, 0 );
	gtk_box_pack_start ( ( GtkBox * ) box, box_path, FALSE, FALSE, 0 );
	gtk_widget_set_margin_top ( box_path, 32 );
	gtk_widget_set_margin_start ( box_path, 32 );
	gtk_widget_set_margin_end ( box_path, 32 );
	gtk_widget_set_margin_top ( label_path, 8 );
	gtk_widget_set_margin_start ( label_path, 8 );
	gtk_widget_set_margin_end ( label_path, 8 );
	gtk_widget_set_margin_bottom ( label_path, 8 );
	gtk_widget_set_margin_top ( w.entry_ap, 8 );
	gtk_widget_set_margin_start ( w.entry_ap, 8 );
	gtk_widget_set_margin_bottom ( w.entry_ap, 8 );
	gtk_widget_set_margin_top ( w.button_ap, 8 );
	gtk_widget_set_margin_start ( w.button_ap, 8 );
	gtk_widget_set_margin_end ( w.button_ap, 8 );
	gtk_widget_set_margin_bottom ( w.button_ap, 8 );

	w.entry_width_ap = gtk_entry_new ( );
	w.entry_height_ap = gtk_entry_new ( );
	gtk_entry_set_width_chars ( ( GtkEntry * ) w.entry_width_ap, 4 );
	gtk_entry_set_width_chars ( ( GtkEntry * ) w.entry_height_ap, 4 );
	GtkWidget *box_wh = gtk_box_new ( GTK_ORIENTATION_HORIZONTAL, 0 );
	GtkWidget *label_width = gtk_label_new ( "width" );
	GtkWidget *label_height = gtk_label_new ( "height" );
	gtk_entry_set_text ( ( GtkEntry * ) w.entry_width_ap, "32" );
	gtk_entry_set_text ( ( GtkEntry * ) w.entry_height_ap, "32" );
	gtk_box_pack_start ( ( GtkBox * ) box_wh, label_width, FALSE, FALSE, 0 );
	gtk_box_pack_start ( ( GtkBox * ) box_wh, w.entry_width_ap, FALSE, FALSE, 0 );
	gtk_box_pack_start ( ( GtkBox * ) box_wh, label_height, FALSE, FALSE, 0 );
	gtk_box_pack_start ( ( GtkBox * ) box_wh, w.entry_height_ap, FALSE, FALSE, 0 );
	gtk_widget_set_margin_top ( box_wh, 8 );
	gtk_widget_set_margin_start ( box_wh, 32 );
	gtk_widget_set_margin_end ( box_wh, 32 );
	gtk_widget_set_margin_bottom ( box_wh, 32 );
	gtk_widget_set_margin_top ( label_width, 8 );
	gtk_widget_set_margin_bottom ( label_width, 8 );
	gtk_widget_set_margin_start ( label_width, 8 );
	gtk_widget_set_margin_top ( label_height, 8 );
	gtk_widget_set_margin_start ( label_height, 8 );
	gtk_widget_set_margin_end ( label_height, 8 );
	gtk_widget_set_margin_bottom ( label_height, 8 );
	gtk_widget_set_margin_top ( w.entry_width_ap, 8 );
	gtk_widget_set_margin_start ( w.entry_width_ap, 8 );
	gtk_widget_set_margin_bottom ( w.entry_width_ap, 8 );
	gtk_widget_set_margin_top ( w.entry_height_ap, 8 );
	gtk_widget_set_margin_bottom ( w.entry_height_ap, 8 );
	gtk_widget_set_margin_start ( w.entry_height_ap, 8 );

	gtk_box_pack_start ( ( GtkBox * ) box, box_wh, FALSE, FALSE, 0 );

	w.button_add_ap = gtk_button_new_with_label ( "Добавить" );

	g_signal_connect ( w.button_add_ap, "clicked", G_CALLBACK ( button_add_ap_cb ), NULL );
	g_signal_connect ( w.button_ap, "clicked", G_CALLBACK ( button_ap_cb ), NULL );

	gtk_box_pack_start ( ( GtkBox * ) box, w.button_add_ap, FALSE, FALSE, 0 );
	gtk_widget_set_margin_top ( w.button_add_ap, 8 );
	gtk_widget_set_margin_bottom ( w.button_add_ap, 8 );
	gtk_widget_set_margin_start ( w.button_add_ap, 8 );
	gtk_widget_set_margin_end ( w.button_add_ap, 8 );
	
}

static void static_create_app_menu ( ) { 
	const GActionEntry entries[] = {
		{ "new_project", activate_new_project },
		{ "save_project", activate_save_project },
		{ "save_as_project", activate_save_as_project },
		{ "open_project", activate_open_project },
		{ "add_picture", activate_add_picture },
		{ "quit", activate_quit }
	};

	g_action_map_add_action_entries ( G_ACTION_MAP ( app ), entries, G_N_ELEMENTS ( entries ), NULL );

	GMenu *menu_root = g_menu_new ( );
	GMenu *menu_project = g_menu_new ( );
	g_menu_append ( menu_project, "Новый проект", "app.new_project" );
	g_menu_append ( menu_project, "Открыть проект", "app.open_project" );
	g_menu_append ( menu_project, "Сохранить проект", "app.save_project" );
	g_menu_append ( menu_project, "Сохранить проект как ...", "app.save_as_project" );
	g_menu_append_submenu ( menu_root, "Проект", ( GMenuModel * ) menu_project );
	g_menu_append ( menu_root, "Добавить Sprite Sheet", "app.add_picture" );
	g_menu_append ( menu_root, "Выход", "app.quit" );

	gtk_application_set_app_menu ( ( GtkApplication * ) app, ( GMenuModel * ) menu_root );
}

static void clear_button_toggled_cb ( GtkToggleButton *button, gpointer data ) {
	if ( lm ) {
		lm->state_cursor = STATE_CLEAR_BUTTON;
		gtk_toggle_button_set_active ( ( GtkToggleButton * ) w.cursor_button, FALSE );
	}
}
static void cursor_button_toggled_cb ( GtkToggleButton *button, gpointer data ) {
	if ( lm ) {
		lm->state_cursor = STATE_CURSOR_BUTTON;
		gtk_toggle_button_set_active ( ( GtkToggleButton * ) w.clear_button, FALSE );
	}
}

static void app_activate_cb ( GtkApplication *app, gpointer data ) {
	static_init_list_files ( );

	w.window = gtk_application_window_new ( app );
	gtk_window_maximize ( ( GtkWindow * ) w.window );
	w.box = gtk_box_new ( GTK_ORIENTATION_VERTICAL, 0 );
	gtk_container_add ( ( GtkContainer * ) w.window, w.box );

	static_init_add_picture ( );

	w.notebook = gtk_notebook_new ( );
	gtk_box_pack_start ( ( GtkBox * ) w.box, w.notebook, TRUE, TRUE, 0 );

	w.header = gtk_header_bar_new ( );
	gtk_header_bar_set_show_close_button ( ( GtkHeaderBar * ) w.header, TRUE );
	gtk_header_bar_set_decoration_layout ( ( GtkHeaderBar * ) w.header, ":menu,minimize,maximize,close" );
	gtk_window_set_titlebar ( ( GtkWindow * ) w.window, w.header );

	w.clear_button = gtk_toggle_button_new_with_label ( "Ластик" );
	w.cursor_button = gtk_toggle_button_new_with_label ( "Курсор" );

	g_signal_connect ( w.clear_button, "toggled", G_CALLBACK ( clear_button_toggled_cb ), NULL );
	g_signal_connect ( w.cursor_button, "toggled", G_CALLBACK ( cursor_button_toggled_cb ), NULL );

	gtk_header_bar_pack_end ( ( GtkHeaderBar * ) w.header, w.cursor_button );
	gtk_header_bar_pack_end ( ( GtkHeaderBar * ) w.header, w.clear_button );

	static_create_app_menu ( );

	gtk_widget_show_all ( w.window );
}

int main ( int argc, char **argv ) {
	app = gtk_application_new ( "org.xverizex.gtiles", G_APPLICATION_FLAGS_NONE );
	g_application_register ( ( GApplication * ) app, NULL, NULL );
	g_signal_connect ( app, "activate", G_CALLBACK ( app_activate_cb ), NULL );
	return g_application_run ( ( GApplication * ) app, argc, argv );
}
