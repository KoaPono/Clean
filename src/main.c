#include <pebble.h>

// Variables
static Window *s_main_window;

static TextLayer *s_hour_layer;
static TextLayer *s_min_layer;
static TextLayer *s_day_layer;
static TextLayer *s_date_layer;
static TextLayer *s_temp_layer;

static Layer *s_battery_layer;

static BitmapLayer *s_wthr_icon_layer;
static BitmapLayer *s_btry_icon_layer;

static GFont s_hour_font;
static GFont s_min_font;
static GFont s_day_font;

static GBitmap *s_wthr_icon_bitmap;
static GBitmap *s_btry_icon_bitmap;

static int s_battery_level;
static bool vibrateEveryHour;
// End Variables

// js Event Functions
static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
	// Store incoming information
	static char temperature_buffer[8];
	static char conditions_buffer[32];
	static char weather_layer_buffer[32];
	
	// Read tuples for data
	Tuple *temp_tuple = dict_find(iterator, MESSAGE_KEY_TEMPERATURE);
	Tuple *conditions_tuple = dict_find(iterator, MESSAGE_KEY_CONDITIONS);

	// If all data is available, use it
	if(temp_tuple || conditions_tuple) {
  		snprintf(temperature_buffer, sizeof(temperature_buffer), "%d°F", (int)temp_tuple->value->int32);
  		snprintf(conditions_buffer, sizeof(conditions_buffer), "%s", conditions_tuple->value->cstring);
		
		// Assemble full string and display
		//snprintf(weather_layer_buffer, sizeof(weather_layer_buffer), "%s %s", temperature_buffer, conditions_buffer);
		snprintf(weather_layer_buffer, sizeof(weather_layer_buffer), "%s", temperature_buffer);
		text_layer_set_text(s_temp_layer, weather_layer_buffer);
	}
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}
// End js Event Functions

// Battery functions
static void battery_callback(BatteryChargeState state) {
	// Record the new battery level
	s_battery_level = state.charge_percent;
	
	// Update meter
	layer_mark_dirty(s_battery_layer);
}

static void battery_update_proc(Layer *layer, GContext *ctx) {
	GRect bounds = layer_get_bounds(layer);
	
	//Find the width of the bar
	int width = (int)(float)(((float)s_battery_level / 100.0f) * 25.0f);
	
	// Draw the background
	graphics_context_set_fill_color(ctx, GColorBlack);
	graphics_fill_rect(ctx, bounds, 0, GCornerNone);
	
	//Draw the bar
	graphics_context_set_fill_color(ctx, GColorWhite);
	graphics_fill_rect(ctx, GRect(0, 0, width, bounds.size.h), 0, GCornerNone);
}
// End Battery Functions

// Time and Date Functions
static void UpdateTime(){
	// Get a tm struct
	time_t temp = time(NULL);
	struct tm *tick_time = localtime(&temp);
	
	// Write the current hours and minutes into the buffer
	static char hBuffer[4];
	static char mBuffer[4];
	
	strftime(hBuffer, sizeof(hBuffer), clock_is_24h_style() ? "%H" : "%I", tick_time);
	strftime(mBuffer, sizeof(mBuffer), ":%M", tick_time);
	
	//Dispay this time on the TextLayer
	text_layer_set_text(s_hour_layer, hBuffer);
	text_layer_set_text(s_min_layer,  mBuffer);
}

static void UpdateDate() {
	// Get a tm struct
	time_t temp = time(NULL);
	struct tm *tick_time = localtime(&temp);
	
	static char dBuffer[12];
	static char dtBuffer[8];
	
	strftime(dBuffer, sizeof(dBuffer), "%A", tick_time);
	strftime(dtBuffer, sizeof(dtBuffer), "%b %d", tick_time);
	
	text_layer_set_text(s_day_layer,  dBuffer);
	text_layer_set_text(s_date_layer, dtBuffer);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed){
	if ((units_changed & MINUTE_UNIT) != 0) {
		UpdateTime();
	}
	
	if ((units_changed & DAY_UNIT) != 0) {
		UpdateDate();
	}
	
	if ((units_changed & HOUR_UNIT) != 0 && vibrateEveryHour) {
		vibes_short_pulse();
	}
	
	// Get weather update every 30 minutes
	if(tick_time->tm_min % 30 == 0) {
		//vibes_short_pulse();
		
  		// Begin dictionary
  		DictionaryIterator *iter;
 	 	app_message_outbox_begin(&iter);

  		// Add a key-value pair
  		dict_write_uint8(iter, 0, 0);

  		// Send the message!
  		app_message_outbox_send();
	}
}
// End Time and Date Functions

static void main_window_load(Window *window) {
	// Get information about the Window
	Layer *window_layer = window_get_root_layer(window);
	GRect bounds = layer_get_bounds(window_layer);
	// Change background color
	window_set_background_color(window, GColorBlack);
	
	// Create Layers
	s_hour_layer = text_layer_create(GRect(-5 , 30, 75, 82));
	s_min_layer  = text_layer_create(GRect(80, 42, 60, 44));
	s_day_layer  = text_layer_create(GRect(0 , 10, 144, 30));
	s_date_layer = text_layer_create(GRect(80, 85, 60, 30));
	s_temp_layer = text_layer_create(GRect(5, 125, 50, 30));
	
	// Create GBitmap
	s_wthr_icon_bitmap = gbitmap_create_with_resource(RESOURCE_ID_PARTLY_CLOUDY_IMG);
	s_btry_icon_bitmap = gbitmap_create_with_resource(RESOURCE_ID_BATTERY_IMG);
	// Create BitmapLayer to display the GBitmap
	s_wthr_icon_layer = bitmap_layer_create(GRect(60, 127, 30, 30));
	s_btry_icon_layer = bitmap_layer_create(GRect(96, 127, 30, 30));
	
	// Create battery meter Layer
	s_battery_layer = layer_create(GRect(98, 138, 25, 10));
	layer_set_update_proc(s_battery_layer, battery_update_proc);
	
	s_hour_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_BEBAS_FONT_80));
	s_min_font  = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_BEBAS_FONT_42));
	s_day_font  = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_BEBAS_FONT_25));
	
	text_layer_set_background_color(s_day_layer, GColorClear);
	text_layer_set_text_color		 (s_day_layer, GColorWhite);
	text_layer_set_font				 (s_day_layer, s_day_font);
	text_layer_set_text_alignment  (s_day_layer, GTextAlignmentCenter);
	
	text_layer_set_background_color(s_hour_layer, GColorClear);
	text_layer_set_text_color		 (s_hour_layer, GColorWhite);
	text_layer_set_text				 (s_hour_layer, "23");
	text_layer_set_font				 (s_hour_layer, s_hour_font);
	text_layer_set_text_alignment  (s_hour_layer, GTextAlignmentRight);
	
	text_layer_set_background_color(s_min_layer, GColorClear);
	text_layer_set_text_color		 (s_min_layer, GColorWhite);
	text_layer_set_text				 (s_min_layer, ":55");
	text_layer_set_font				 (s_min_layer, s_min_font);
	text_layer_set_text_alignment  (s_min_layer, GTextAlignmentLeft);
	
	text_layer_set_background_color(s_date_layer, GColorClear);
	text_layer_set_text_color		 (s_date_layer, GColorWhite);
	text_layer_set_font				 (s_date_layer, s_day_font);
	text_layer_set_text				 (s_date_layer, "000 00");
	text_layer_set_text_alignment  (s_date_layer, GTextAlignmentLeft);
	
	text_layer_set_background_color(s_temp_layer, GColorClear);
	text_layer_set_text_color		 (s_temp_layer, GColorWhite);
	text_layer_set_font				 (s_temp_layer, s_day_font);
	text_layer_set_text				 (s_temp_layer, "-°F");
	text_layer_set_text_alignment  (s_temp_layer, GTextAlignmentRight);
	
	// Set the bitmap onto the layer and add to the window
	bitmap_layer_set_bitmap(s_wthr_icon_layer, s_wthr_icon_bitmap);
	bitmap_layer_set_bitmap(s_btry_icon_layer, s_btry_icon_bitmap);
	
	// Add as child layers to the Window's root layer
	layer_add_child(window_layer, text_layer_get_layer(s_hour_layer));
	layer_add_child(window_layer, text_layer_get_layer(s_min_layer));
	layer_add_child(window_layer, text_layer_get_layer(s_day_layer));
	layer_add_child(window_layer, text_layer_get_layer(s_date_layer));
	layer_add_child(window_layer, text_layer_get_layer(s_temp_layer));
	
	layer_add_child(window_layer, bitmap_layer_get_layer(s_wthr_icon_layer));
	layer_add_child(window_layer, bitmap_layer_get_layer(s_btry_icon_layer));
	layer_add_child(window_layer, s_battery_layer);
}

static void main_window_unload(Window *window){
	fonts_unload_custom_font(s_hour_font);
	fonts_unload_custom_font(s_min_font);
	fonts_unload_custom_font(s_day_font);

	text_layer_destroy(s_hour_layer);
	text_layer_destroy(s_min_layer);
	text_layer_destroy(s_day_layer);
	text_layer_destroy(s_temp_layer);
	
	gbitmap_destroy(s_wthr_icon_bitmap);
	gbitmap_destroy(s_btry_icon_bitmap);
	bitmap_layer_destroy(s_wthr_icon_layer);
	bitmap_layer_destroy(s_btry_icon_layer);
	
	layer_destroy(s_battery_layer);
}

static void init() {
	vibrateEveryHour = true;
	
	// Create main Window element and assign to pointer
	s_main_window = window_create();

	// Set handlers to manage the elements inside the Window
	window_set_window_handlers(s_main_window, (WindowHandlers) {
		.load = main_window_load,
		.unload = main_window_unload
	});

	// Show the Window on the watch, with animated=true
	window_stack_push(s_main_window, true);
	
	// Register with tickTimerService
	tick_timer_service_subscribe(MINUTE_UNIT | DAY_UNIT | HOUR_UNIT, tick_handler);
	
	// Make sure the time is displayed from the start
	UpdateTime();
	UpdateDate();
	
	// Register callbacks
	app_message_register_inbox_received(inbox_received_callback);
	
	// Register for battery level updates
	battery_state_service_subscribe(battery_callback);
	
	// Ensure battery level is displayed from the start
	battery_callback(battery_state_service_peek());
	
	// Open AppMessage
	const int inbox_size = 128;
	const int outbox_size = 128;
	app_message_open(inbox_size, outbox_size);
	
	app_message_register_inbox_dropped(inbox_dropped_callback);
	app_message_register_outbox_failed(outbox_failed_callback);
	app_message_register_outbox_sent(outbox_sent_callback);
}

static void deinit() {
	// Destroy Window
	window_destroy(s_main_window);
}

int main(void) {
	init();
	app_event_loop();
	deinit();
}
