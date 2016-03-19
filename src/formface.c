#include <pebble.h>
#include <ctype.h>

#define KEY_COLOR_1 1
#define KEY_COLOR_2 2
#define KEY_COLOR_3 3
#define KEY_TRI_COLOR 4
#define KEY_SHOW_DATE 5
#define KEY_FORMAT_12 6

#define PERSISTED_FALSE 30
#define DATE_SHIFT 18

#define ANIMATION_DURATION 500

// #define FAST_ANIM_MODE
#ifdef FAST_ANIM_MODE
  #define TICK_UNIT SECOND_UNIT
#else
  #define TICK_UNIT MINUTE_UNIT 
#endif

#ifdef PBL_COLOR
  static GColor COLOR_1, COLOR_2, COLOR_3;

  #define SET_FILL_COLOR(x) graphics_context_set_fill_color(ctx, x);
  #define SET_STROKE_COLOR(x) graphics_context_set_stroke_color(ctx, x);
  #define SET_FIXED_COLOR

  #define DRAW_RECT_2(x, y, w, h) graphics_fill_rect(ctx, GRect(x, y, w, h), 0, GCornerNone)
  #define DRAW_RECT_3(x, y, w, h) graphics_fill_rect(ctx, GRect(x, y, w, h), 0, GCornerNone)

  #define DRAW_CIRCLE_2(x, y, r) graphics_fill_circle(ctx, GPoint(x, y), r);
  #define DRAW_CIRCLE_3(x, y, r) graphics_fill_circle(ctx, GPoint(x, y), r);

#else
  #define COLOR_1 GColorWhite
  #define COLOR_2 GColorWhite
  #define COLOR_3 GColorWhite

  #define SET_FILL_COLOR(x)
  #define SET_STROKE_COLOR(x)
  #define SET_FIXED_COLOR \
    graphics_context_set_fill_color(ctx, GColorWhite); \
    graphics_context_set_stroke_color(ctx, GColorWhite);


  // *********** Custom color funtions for black and white pebble.
  typedef void(* PixelImplementation)(GContext* ctx, int x, int y);

  static void gray_color_impl(GContext* ctx, int x, int y) {
    if (((x + y) & 1) == 0) {
      graphics_draw_pixel(ctx, GPoint(x, y));    
    }
  }
  static void light_gray_color_impl(GContext* ctx, int x, int y) {
    if ((x & 1) == 0 || (y & 1) != 0) {
      graphics_draw_pixel(ctx, GPoint(x, y));    
    }
  }
  static void circle_impl_for_6(GContext* ctx, int x, int y) {
    if (y >= 26 && ((x + y) & 1) == 0) {
      graphics_draw_pixel(ctx, GPoint(x, y));    
    }
  }
  static void circle_impl_for_9(GContext* ctx, int x, int y) {
    if (y < 26 && ((x + y) & 1) == 0) {
      graphics_draw_pixel(ctx, GPoint(x, y));    
    }
  }

  static void draw_gray_rect(GContext* ctx, int x1, int y1, int x2, int y2, PixelImplementation impl) {
    int x, y;
    for (x = x1; x < x2; x++) {
      for (y = y1; y < y2; y++) {
        (*impl) (ctx, x, y);
      }
    }
  }

  /**
   * Draws a circle at the center 26, 26. Not the circle is not antialiased.
   */
  #define draw_center_circle(r, ctx, impl) draw_circle(26, 26, r, ctx, impl)
  
  static void draw_circle(int cx, int cy, int r, GContext* ctx, PixelImplementation impl) {
    int x = r;
    int y = 0;
    int xChange = 1 - (r << 1);
    int yChange = 0;
    int radiusError = 0;
    int i;
  
    while (x >= y) {
      for (i = cx - x; i <= cx + x; i++) {
        (*impl) (ctx, i, cy + y);
        (*impl) (ctx, i, cy - y);
      }
      for (i = cx - y; i <= cx + y; i++) {
        (*impl) (ctx, i, cy + x);
        (*impl) (ctx, i, cy - x);
      }
      y++;
      radiusError += yChange;
      yChange += 2;
  
      if (((radiusError << 1) + xChange) > 0) {
        x--;
        radiusError += xChange;
        xChange += 2;
      }
    }
  }

  static bool TRI_COLOR = true;

  #define DRAW_RECT_2(x, y, w, h) draw_gray_rect(ctx, x, y, w + x, h + y, &gray_color_impl)
  #define DRAW_RECT_3(x, y, w, h) draw_gray_rect(ctx, x, y, w + x, h + y, TRI_COLOR ? &light_gray_color_impl : &gray_color_impl)

  #define DRAW_CIRCLE_2(x, y, r) draw_circle(x, y, r, ctx, &gray_color_impl);
  #define DRAW_CIRCLE_3(x, y, r) \
    if (TRI_COLOR) \
      draw_circle(x, y, r, ctx, &light_gray_color_impl); \
    else \
      graphics_fill_circle(ctx, GPoint(x, y), r);

#endif

static bool SHOW_DATE = true;
static bool FORMAT_12 = true;

// Various paths
static const GPathInfo TWO_PATH_INFO = {
  .num_points = 3,
  .points = (GPoint []) {{26, 26}, {0, 52}, {26, 52}}
};
static const GPathInfo THREE_PATH_INFO = {
  .num_points = 4,
  .points = (GPoint []) {{52, 0}, {34, 17}, {0, 17}, {0, 0}}
};
static const GPathInfo FOUR_PATH_INFO = {
  .num_points = 3,
  .points = (GPoint []) {{26, 0}, {0, 26}, {26, 26}}
};
static const GPathInfo SIX_PATH_INFO = {
  .num_points = 4,
  .points = (GPoint []) {{13, 0}, {39, 0}, {26, 25}, {0, 25}}
};
static const GPathInfo SEVEN_PATH_INFO = {
  .num_points = 4,
  .points = (GPoint []) {{26, 0}, {52, 0}, {26, 52}, {0, 52}}
};
static const GPathInfo NINE_PATH_INFO = {
  .num_points = 4,
  .points = (GPoint []) {{26, 26}, {52, 26}, {39, 52}, {13, 52}}
};

static GPath *s_two_path_ptr, *s_three_path_ptr, *s_four_path_ptr, *s_six_path_ptr, *s_seven_path_ptr, *s_nine_path_ptr;

typedef struct digit_layer_data_struct {
  int lastValue;
  int currentValue;

  int progress;
  Animation* anim;
  const AnimationImplementation* anim_handler;
} digit_layer_data;

static Window *s_main_window;
static Layer *s_layer_dots;

static TextLayer *s_date_layer;
#ifdef PBL_COLOR
static TextLayer *s_date_layer2;
#endif

/*********************************** Animation **************************************/
static void digit_layer_animation_update(Layer *layer, AnimationProgress dist_normalized) {
  digit_layer_data* layer_data =  (digit_layer_data*) layer_get_data(layer);
  layer_data->progress = (int)(float)(((float)dist_normalized / (float)ANIMATION_NORMALIZED_MAX) * (float)100);
  layer_mark_dirty(layer);
}

#define DIGIT_LAYER(NAME) \
  static Layer * s_ ## NAME; \
  static void NAME ## _animation_update(Animation *anim, AnimationProgress dist_normalized) { \
    digit_layer_animation_update(s_ ## NAME, dist_normalized); \
  } \
  static const AnimationImplementation NAME ## _animation_impl = { \
    .update = NAME ## _animation_update \
  };

DIGIT_LAYER(layer_h1);
DIGIT_LAYER(layer_h2);
DIGIT_LAYER(layer_m1);
DIGIT_LAYER(layer_m2);


/*********************************** Drawing **************************************/
static void draw_digit_layer_with_progress(GContext* ctx, int data, int progress) {
  int shift;
  SET_FIXED_COLOR;
  switch (data) {
    case 0: {
      shift = 25 * progress / 100;
      SET_FILL_COLOR(COLOR_3);
      DRAW_CIRCLE_2(26, 26, shift);

      SET_FILL_COLOR(COLOR_1);
      graphics_fill_radial(ctx, GRect(26 - shift, 26 - shift, shift + shift + 1, shift + shift + 1), GOvalScaleModeFitCircle, 40,
                           DEG_TO_TRIGANGLE(progress * 90 / 100 - 45),  DEG_TO_TRIGANGLE(progress * 90 / 100 + 135));
      break;
    }
    case 1: {
      SET_FILL_COLOR(COLOR_2);
      DRAW_RECT_2(9, 0, 36 * progress / 100, 18);
    
      if (progress > 50) {
        SET_FILL_COLOR(COLOR_3);
        shift = 34 * progress / 50 - 34;
        graphics_fill_rect(ctx, GRect(17, 52 - shift, 28, shift), 0, GCornerNone);
      }
      break;
    }
    case 2: {
      SET_FILL_COLOR(COLOR_1);
      #ifdef PBL_COLOR
        graphics_fill_rect(ctx, GRect(0, 0, 52 * progress / 100, 26), 13 * progress / 100, GCornersRight);
      #else
        if (TRI_COLOR) {
          draw_circle(39, 12, 13 * progress / 100, ctx, &light_gray_color_impl);
          draw_gray_rect(ctx, 0, 0, 39 * progress / 100, 26, &light_gray_color_impl);
        } else {
          graphics_fill_circle(ctx, GPoint(39, 12), 13 * progress / 100);
          graphics_fill_rect(ctx, GRect(0, 0, 39 * progress / 100, 26), 0, GCornerNone);
        }
      #endif

      if (progress > 50) {
        SET_FILL_COLOR(COLOR_3);
        gpath_move_to(s_two_path_ptr, GPoint(52 - 26 * progress / 50, 0));
        gpath_draw_filled(ctx, s_two_path_ptr);
      }

      SET_FILL_COLOR(COLOR_2);
      if (progress < 50) {
        shift = 26 * progress / 50;
        DRAW_RECT_2(26, 52 - shift, 26, shift);
      } else {
        DRAW_RECT_2(26, 26, 26, 26);
      }
      break;
    }
    case 3: {
      SET_FILL_COLOR(COLOR_3);
      gpath_move_to(s_three_path_ptr, GPoint(52 * progress / 100 - 52, 0));
      gpath_draw_filled(ctx, s_three_path_ptr);
      DRAW_CIRCLE_2(34, 35, (progress < 50) ? (17 * progress / 50) : 17);
    
      if (progress > 50) {
        shift = 34 * progress / 50 - 34;
        SET_FILL_COLOR(COLOR_2);
        DRAW_RECT_2(shift - 20, 18, 20, 17);
      
        SET_FILL_COLOR(COLOR_1);
        graphics_fill_rect(ctx, GRect(0, 34, shift, 18), 0, GCornerNone);
      }
      break;
    }
    case 4: {
      if (progress > 50) {
        SET_FILL_COLOR(COLOR_2);
        shift = 52 - 26 * progress / 50;
        gpath_move_to(s_four_path_ptr, GPoint(shift, shift));
        gpath_draw_filled(ctx, s_four_path_ptr);
        graphics_fill_rect(ctx, GRect(26, 39, 26, 13), 0, GCornerNone);
      
        SET_FILL_COLOR(COLOR_1);
        DRAW_RECT_2(shift, 26, 52, 13);
      }

      SET_FILL_COLOR(COLOR_3);
      DRAW_RECT_3(26, 52 - 52 * progress / 100, 26, 26);
      break;
    }
    case 5: {
      if (progress > 50) {
        SET_FILL_COLOR(COLOR_3);
        DRAW_CIRCLE_2(34 * progress / 100, 35, 17);
      }
    
      SET_FILL_COLOR(COLOR_2);
      if (progress > 50) {
        graphics_fill_rect(ctx, GRect(34, 0, 18 * progress / 50 - 18, 18), 0, GCornerNone);
      }
      if (progress > 30) {
        graphics_fill_rect(ctx, GRect(0, 34, 34, 19), 0, GCornerNone);
      }
    
      SET_FILL_COLOR(COLOR_1);
      shift = (progress < 52) ? (52 - progress) : 0;
      DRAW_RECT_3(0, shift, 34, 34);
      break;
    }
    case 6: {
      #ifdef PBL_COLOR
      SET_FILL_COLOR(COLOR_3);
      graphics_fill_radial(ctx, GRect(0, 0, 51, 51), GOvalScaleModeFitCircle, 40,
                           DEG_TO_TRIGANGLE(90),  DEG_TO_TRIGANGLE(progress * 180 / 100 + 90));
      #else
      draw_center_circle(progress / 4, ctx, &circle_impl_for_6);
      #endif

      SET_FILL_COLOR(COLOR_2);
      shift = 13 * progress / 100;
      gpath_move_to(s_six_path_ptr, GPoint(12 - shift, 2 * shift - 26));
      gpath_draw_filled(ctx, s_six_path_ptr);
      break;
    }
    case 7: {
      SET_FILL_COLOR(COLOR_3);
      DRAW_RECT_2(0, 0, 30 * progress / 100, 26);
    
      shift = 26 * progress / 100;
      SET_FILL_COLOR(COLOR_2);
      gpath_move_to(s_seven_path_ptr, GPoint(26 - shift, 2 * shift - 52));
      gpath_draw_filled(ctx, s_seven_path_ptr);
      break;
    }
    case 8: {
      if (progress > 50) {
        SET_FILL_COLOR(COLOR_3);
        shift = 34 * progress / 50 - 34;
        graphics_fill_rect(ctx, GRect((52 - shift) / 2, 0, shift, 18), 9, GCornersAll);

        SET_FILL_COLOR(COLOR_2);
        DRAW_CIRCLE_3(34, 34, 16 * progress / 50 - 16);
      }
    
      SET_FILL_COLOR(COLOR_1);
      #ifdef PBL_COLOR
        graphics_fill_rect(ctx, GRect(0, 18, (progress < 50) ? (34 * progress / 50) : 34, 34), 16, GCornersLeft);
      #else
        DRAW_CIRCLE_2(16, 34, (progress < 50) ? (16 * progress / 50) : 16);
        if (progress > 50) {
          shift = 16 * progress / 50 - 16;
          graphics_context_set_fill_color(ctx, GColorBlack);
          graphics_fill_rect(ctx, GRect(16, 18, shift, 34), 0, GCornerNone);
          DRAW_RECT_2(16, 18, shift, 34);
        }
      #endif
      break;
    }
    case 9: {
      #ifdef PBL_COLOR
      SET_FILL_COLOR(COLOR_3);
      graphics_fill_radial(ctx, GRect(0, 0, 51, 51), GOvalScaleModeFitCircle, 40,
                           DEG_TO_TRIGANGLE(-90),  DEG_TO_TRIGANGLE(progress * 180 / 100 - 90));
      #else
      draw_center_circle(progress / 4, ctx, &circle_impl_for_9);
      #endif

      SET_FILL_COLOR(COLOR_2);
      shift = 13 * progress / 100;
      gpath_move_to(s_nine_path_ptr, GPoint(shift - 13, 25 - 2 * shift));
      gpath_draw_filled(ctx, s_nine_path_ptr);
      break;
    }
  }
}

static void digit_layer_update_callback(Layer *layer, GContext* ctx) {
  digit_layer_data* data = (digit_layer_data*) layer_get_data(layer);
  if (data->progress < 100) {
    draw_digit_layer_with_progress(ctx, data->lastValue, 100 - data->progress);
  }
  draw_digit_layer_with_progress(ctx, data->currentValue, data->progress);
}

static void update_digit_layer(int value, Layer *layer) {
  digit_layer_data* data = (digit_layer_data*) layer_get_data(layer);
  if (data->currentValue != value) {
    int anim_delay = (data->lastValue < -50) ? -data->lastValue : 0;
    data->lastValue = data->currentValue;
    data->currentValue = value;
    data->progress = 0;

    #ifdef PBL_PLATFORM_APLITE
      if (data->anim) {
        animation_destroy(data->anim);
      } 
    #else
      if (data->anim && animation_is_scheduled(data->anim)) {
        animation_destroy(data->anim);
      }
    #endif
    
    Animation *anim = animation_create();
    animation_set_duration(anim, ANIMATION_DURATION);
    animation_set_delay(anim, anim_delay);
    animation_set_curve(anim, AnimationCurveEaseInOut);
    animation_set_implementation(anim, data->anim_handler);
    data->anim = anim;
    animation_schedule(data->anim);
  }
}



static void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed) {

  #ifdef FAST_ANIM_MODE
    update_digit_layer(tick_time->tm_min / 10, s_layer_h1);
    update_digit_layer(tick_time->tm_min % 10, s_layer_h2);
    update_digit_layer(tick_time->tm_sec / 10, s_layer_m1);
    update_digit_layer(tick_time->tm_sec % 10, s_layer_m2);
  #else
    int hr = tick_time->tm_hour;
    if (FORMAT_12 && hr > 12) {
      hr = hr - 12;
    }
    update_digit_layer(hr / 10, s_layer_h1);
    update_digit_layer(hr % 10, s_layer_h2);
    update_digit_layer(tick_time->tm_min / 10, s_layer_m1);
    update_digit_layer(tick_time->tm_min % 10, s_layer_m2);
  #endif
  

  static char s_date_text[] = "WED 26";
  strftime(s_date_text, sizeof(s_date_text), "%a %e", tick_time);
  s_date_text[1] = toupper((int) s_date_text[1]);
  s_date_text[2] = toupper((int) s_date_text[2]);
  
  text_layer_set_text(s_date_layer, s_date_text);
  #ifdef PBL_COLOR
  text_layer_set_text(s_date_layer2, s_date_text);
  #endif
  
}

static Layer *create_digit_layer(Layer* window_layer, int x, int y,
                                 const AnimationImplementation* anim_handler,
                                 int initial_anim_delay) {
  Layer *layer = layer_create_with_data(GRect(x, y + (SHOW_DATE ? 0 : DATE_SHIFT), 52, 52), sizeof(digit_layer_data));
  digit_layer_data* layer_data =  (digit_layer_data*) layer_get_data(layer);
  layer_data->currentValue = -1;

  // Store the initial delay in lastValue, this is overriden ofter the first animation.
  layer_data->lastValue = -initial_anim_delay;
  layer_data->progress = 0;
  layer_data->anim_handler = anim_handler;

  layer_set_update_proc(layer, digit_layer_update_callback);
  layer_add_child(window_layer, layer);
  return layer;
}

static void dots_layer_update_callback(Layer *layer, GContext* ctx) {
  SET_FIXED_COLOR;
  SET_FILL_COLOR(COLOR_2);
  DRAW_CIRCLE_2(5, 15, 5);

  SET_FILL_COLOR(COLOR_3);
  graphics_fill_circle(ctx, GPoint(5, 35), 5);
}



#define INIT_VAL(NAME, FUNCTION, DEFAULT) NAME = persist_read_int(KEY_ ## NAME) ? FUNCTION(persist_read_int(KEY_ ## NAME)) : DEFAULT
#define INT_TO_BOOL(x) x != PERSISTED_FALSE

static TextLayer* createTextLayer(Layer* window_layer, GFont font, GColor color) {
  TextLayer* layer = text_layer_create(GRect(0, 145, 144, 23));
  text_layer_set_text_color(layer, color);
  text_layer_set_background_color(layer, GColorClear);
  text_layer_set_text_alignment(layer, GTextAlignmentCenter);
  text_layer_set_font(layer, font);
  layer_set_hidden(text_layer_get_layer(layer), !SHOW_DATE);
  layer_add_child(window_layer, text_layer_get_layer(layer));
  return layer;
}

static void main_window_load(Window *window) {
  #ifdef PBL_COLOR
    // Init colors
    INIT_VAL(COLOR_1, GColorFromHEX, GColorPictonBlue);
    INIT_VAL(COLOR_2, GColorFromHEX, GColorBlue);
    INIT_VAL(COLOR_3, GColorFromHEX, GColorWhite);
  #else
    INIT_VAL(TRI_COLOR, INT_TO_BOOL, true);
  #endif

  INIT_VAL(SHOW_DATE, INT_TO_BOOL, true);
  INIT_VAL(FORMAT_12, INT_TO_BOOL, true);

  Layer *window_layer = window_get_root_layer(window);

  // Create paths
  s_two_path_ptr = gpath_create(&TWO_PATH_INFO);
  s_three_path_ptr = gpath_create(&THREE_PATH_INFO);
  s_four_path_ptr = gpath_create(&FOUR_PATH_INFO);
  s_six_path_ptr = gpath_create(&SIX_PATH_INFO);
  s_seven_path_ptr = gpath_create(&SEVEN_PATH_INFO);
  s_nine_path_ptr = gpath_create(&NINE_PATH_INFO);

  // Dots layer
  s_layer_dots = layer_create(GRect(124, 10 + (SHOW_DATE ? 0 : DATE_SHIFT), 12, 52));
  layer_set_update_proc(s_layer_dots, dots_layer_update_callback);
  layer_add_child(window_layer, s_layer_dots);

  // Digit layers
  s_layer_h1 = create_digit_layer(window_layer, 4, 10, &layer_h1_animation_impl, 0);
  s_layer_h2 = create_digit_layer(window_layer, 64, 10, &layer_h2_animation_impl, 150);
  s_layer_m1 = create_digit_layer(window_layer, 28, 70, &layer_m1_animation_impl, 300);
  s_layer_m2 = create_digit_layer(window_layer, 88, 70, &layer_m2_animation_impl, 450);

  // Date layer
  s_date_layer = createTextLayer(window_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_MATERIAL_FONT_BOTTOM_16)), COLOR_1);
  #ifdef PBL_COLOR
  s_date_layer2 = createTextLayer(window_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_MATERIAL_FONT_TOP_16)), COLOR_3);
  #endif

  tick_timer_service_subscribe(TICK_UNIT, handle_minute_tick);

  // Prevent starting blank
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  handle_minute_tick(t, TICK_UNIT);
}

#ifdef PBL_PLATFORM_APLITE
  #define DESTROY_LAYER_DATA(x) \
    digit_layer_data* x ## _data = (digit_layer_data*) layer_get_data(x); \
    if (x ## _data->anim) { \
      animation_destroy(x ## _data->anim); \
    }
#else
  #define DESTROY_LAYER_DATA(x)
#endif

static void main_window_unload(Window *window) {
  DESTROY_LAYER_DATA(s_layer_h1);
  layer_destroy(s_layer_h1);

  DESTROY_LAYER_DATA(s_layer_h2);
  layer_destroy(s_layer_h2);
  
  DESTROY_LAYER_DATA(s_layer_m1);
  layer_destroy(s_layer_m1);
  
  DESTROY_LAYER_DATA(s_layer_m2);
  layer_destroy(s_layer_m2);

  layer_destroy(s_layer_dots);
  text_layer_destroy(s_date_layer);
  #ifdef PBL_COLOR
  text_layer_destroy(s_date_layer2);
  #endif

  gpath_destroy(s_two_path_ptr);
  gpath_destroy(s_three_path_ptr);
  gpath_destroy(s_four_path_ptr);
  gpath_destroy(s_six_path_ptr);
  gpath_destroy(s_seven_path_ptr);
  gpath_destroy(s_nine_path_ptr);

  tick_timer_service_unsubscribe();
}


#define TURPLE_CHECK(NAME, FUNCTION) \
  Tuple * NAME ## _t = dict_find(iter, KEY_ ## NAME); \
  if (NAME ## _t) { \
    persist_write_int(KEY_ ## NAME, NAME ## _t->value->int32); \
    NAME = FUNCTION(NAME ## _t->value->int32); \
  }

static void update_layer_frame(Layer* layer, int y) {
  GRect frame = layer_get_frame(layer);
  frame.origin.y = y + (SHOW_DATE ? 0 : DATE_SHIFT);
  layer_set_frame(layer, frame);
  layer_mark_dirty(layer);
}

// Settings handler
static void inbox_received_handler(DictionaryIterator *iter, void *context) {
  #ifdef PBL_COLOR
    TURPLE_CHECK(COLOR_1, GColorFromHEX);
    TURPLE_CHECK(COLOR_2, GColorFromHEX);
    TURPLE_CHECK(COLOR_3, GColorFromHEX);
  #else
    TURPLE_CHECK(TRI_COLOR, INT_TO_BOOL);
  #endif

  TURPLE_CHECK(SHOW_DATE, INT_TO_BOOL);
  TURPLE_CHECK(FORMAT_12, INT_TO_BOOL);

  update_layer_frame(s_layer_h1, 10);
  update_layer_frame(s_layer_h2, 10);
  update_layer_frame(s_layer_m1, 70);
  update_layer_frame(s_layer_m2, 70);
  update_layer_frame(s_layer_dots, 10);

  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  handle_minute_tick(t, TICK_UNIT);

  layer_set_hidden(text_layer_get_layer(s_date_layer), !SHOW_DATE);
  text_layer_set_text_color(s_date_layer, COLOR_1);
  #ifdef PBL_COLOR
  layer_set_hidden(text_layer_get_layer(s_date_layer2), !SHOW_DATE);
  text_layer_set_text_color(s_date_layer2, COLOR_3);
  #endif
}

static void init() {
  s_main_window = window_create();
  window_set_background_color(s_main_window, GColorBlack);
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload,
  });
  window_stack_push(s_main_window, true);

  app_message_register_inbox_received(inbox_received_handler);
  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
}

static void deinit() {
  window_destroy(s_main_window);
}

int main() {
  init();
  app_event_loop();
  deinit();
}
