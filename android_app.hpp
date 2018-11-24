#ifndef ANDROID_APP_HPP
#define ANDROID_APP_HPP

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <sched.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/resource.h>

#include<sys/ioctl.h>

#include <jni.h>

#include <android/configuration.h>
#include <android/looper.h>
#include <android/native_activity.h>
#include <android/log.h>

#include "pipefile.hpp"

#include <cgl/public.h>
#include <cgl/thread/mutex.hpp>

using namespace cgl;

#define ANDROID_APP_LOG(...) ((void)__android_log_print(ANDROID_LOG_INFO, "android_app", __VA_ARGS__))
#define ANDROID_APP_ERROR(...) ((void)__android_log_print(ANDROID_LOG_ERROR, "android_app", __VA_ARGS__))

#define ANDROID_APP_DEBUG

#ifdef ANDROID_APP_DEBUG
#define ANDROID_APP_LOGV(...) ((void)__android_log_print(ANDROID_LOG_VERBOSE, "android_app", __VA_ARGS__))
#else
#define ANDROID_APP_LOGV(...) ((void)0)
#endif

//android app events
enum ANDROID_APP_EVENT
{
	APP_START,
	APP_STOP,
	APP_RESUME,
	APP_PAUSE,
	APP_DESTROY,
	APP_SAVE,
	APP_CONFIG,
	APP_LOW_MEMORY,

	APP_WINDOW_CREATE,	//set app.window
	APP_WINDOW_CLOSE,	//reset app.window
	APP_WINDOW_ACTIVATE,
	APP_WINDOW_DEACTIVATE,
	APP_WINDOW_RESIZE,
	APP_WINDOW_RECT_CHANGED,
	APP_WINDOW_PAINT,

	APP_INPUT,		//set app.inputQueue
	APP_TOUCH_DOWN,
	APP_TOUCH_UP,
	APP_TOUCH_MOVE,

	APP_KEY_UP,
	APP_KEY_DOWN,
	APP_KEY_PRESS
};

#pragma pack(push, 1)
struct APP_EVENT
{
	int id;
	union{
		void* data;
		struct{//input motion
			int16_t x, y, z, w;
		};
		struct{//input_keyboard
			int32_t key;
			int32_t flag;
		};
		struct{//ARect
			int16_t left, top, right, bottom;
		};
	};
};
#pragma pack(pop)

class android_app;

extern void android_main(android_app* app);

void android_app_post_event(android_app* app, const APP_EVENT& event);
void android_app_post_event(android_app* app, int id);

class android_app
{
public:
	ANativeActivity* activity;
	AConfiguration* config;
	ALooper* looper;
	ANativeWindow* window;

	void* savedState;
	size_t savedStateSize;
	int stateSaved;

	void* userData;

	void (*onEvent)(android_app* app, APP_EVENT& event);

	int activityState;
	bool running;
public:
	pthread_t thread;
	mutex_t mutex;
	cond_t  cond;
public:
	io::pipefile message;
	AInputQueue* inputQueue;
	int destroyed;
public:
	android_app() : activity(null), config(null), looper(null), window(null),
		savedState(null), savedStateSize(0), stateSaved(0),
		userData(null),
		onEvent(null),
		activityState(0),
		thread(), mutex(), cond(),
		message(),
		inputQueue(null),
		destroyed(0)
	{
		running = false;
	}

	void dispose();

	bool pause()const
	{
		return activityState == APP_PAUSE;// || activityState == APP_STOP;
	}

	int process_message()
	{
		APP_EVENT event;
		int bytes;
		ioctl(message.in(), FIONREAD, &bytes);
		while(bytes){
			if(this->read_event(event)){
				//ANDROID_APP_LOGV("process message %d - %p", event.id, event.data);
				this->process_begin(event);
				if(this->onEvent){
					this->onEvent(this, event);
				}
				this->process_end(event);
			}
			ioctl(message.in(), FIONREAD, &bytes);
		}
		ALooper_pollAll(0, NULL, NULL, NULL);
		//如果传感器有数据，立即处理。ident == USER
		//ANDROID_APP_LOG("pollAll %d", ident);
		return 0;
	}

public://log
	void print_config();
public:
	void cond_wait()
	{
		cond.wait(mutex);
	}

public:
	static int on_input(int fd, int events, void* data);
	int on_input_event(AInputEvent* event);
private:
	bool read_event(APP_EVENT& event);
	void process_begin(APP_EVENT& event);
	void process_end(APP_EVENT& event);

	int on_input_motion(AInputEvent* event);
	int on_input_keyboard(AInputEvent* event);
};

void free_saved_state(android_app* app)
{
	//auto_lock lock(app->mutex);
	if (app->savedState != NULL) {
		free(app->savedState);
		app->savedState = NULL;
		app->savedStateSize = 0;
	}
}

void android_app::print_config()
{
	char lang[2], country[2];
	AConfiguration_getLanguage(config, lang);
	AConfiguration_getCountry(config, country);

	ANDROID_APP_LOGV("Config: mcc=%d mnc=%d lang=%c%c cnt=%c%c orien=%d touch=%d dens=%d "
			"keys=%d nav=%d keysHid=%d navHid=%d sdk=%d size=%d long=%d "
			"modetype=%d modenight=%d",
			AConfiguration_getMcc(config),
			AConfiguration_getMnc(config),
			lang[0], lang[1], country[0], country[1],
			AConfiguration_getOrientation(config),
			AConfiguration_getTouchscreen(config),
			AConfiguration_getDensity(config),
			AConfiguration_getKeyboard(config),
			AConfiguration_getNavigation(config),
			AConfiguration_getKeysHidden(config),
			AConfiguration_getNavHidden(config),
			AConfiguration_getSdkVersion(config),
			AConfiguration_getScreenSize(config),
			AConfiguration_getScreenLong(config),
			AConfiguration_getUiModeType(config),
			AConfiguration_getUiModeNight(config));
}

#ifdef ANDROID_APP_DEBUG
const char* input_motion_name(int action)
{
	switch(action){
	case AMOTION_EVENT_ACTION_DOWN://触摸按下
		return "MOTION_DOWN";
	case AMOTION_EVENT_ACTION_UP://触摸弹起
		return "MOTION_UP";
	case AMOTION_EVENT_ACTION_MOVE://触摸移动
		return "MOTION_MOVE";
	case AMOTION_EVENT_ACTION_CANCEL:
		return "MOTION_CACEL";
	case AMOTION_EVENT_ACTION_OUTSIDE:
		return "MOTION_OUTSIDE";
	default:
		break;
	}
	return null;
}
#endif

int android_app::on_input_motion(AInputEvent* inputEvent)
{
	APP_EVENT event;

	//motion source
	switch(AInputEvent_getSource(inputEvent))
	{
	case AINPUT_SOURCE_TOUCHSCREEN:
		//消息来源于触摸屏
		break;
	case AINPUT_SOURCE_TRACKBALL:
		//消息来源于trackball，轨迹球 or 鼠标?
		break;
	default:
		break;
	}

	switch(AMotionEvent_getAction(inputEvent))
	{
	case AMOTION_EVENT_ACTION_DOWN://触摸按下
		event.id = APP_TOUCH_DOWN;
		break;
	case AMOTION_EVENT_ACTION_UP://触摸弹起
		event.id = APP_TOUCH_UP;
		break;
	case AMOTION_EVENT_ACTION_MOVE://触摸移动
		event.id = APP_TOUCH_MOVE;
		break;
	case AMOTION_EVENT_ACTION_CANCEL:
		break;
	case AMOTION_EVENT_ACTION_OUTSIDE:
		break;
	default:
		break;
	}

	//getX()	是表示view相对于自身左上角的x坐标,
	//getRawX()	是表示相对于物理屏幕左上角的x坐标值
	//AMotionEvent_getXPrecision

	size_t count = AMotionEvent_getPointerCount(inputEvent);
	for(int i=0; i < count; ++i){
		event.x = AMotionEvent_getX(inputEvent, i);
		event.y = AMotionEvent_getY(inputEvent, i);
		event.z = i;

		if(this->onEvent){
			this->onEvent(this, event);
		}

		#ifdef ANDROID_APP_DEBUG
		ANDROID_APP_LOGV("%s : index=%d, pointer=%d, flag=%d state=%d x=%d, y=%d\n", 
			input_motion_name(AMotionEvent_getAction(inputEvent)),
			i,
			AMotionEvent_getPointerId(inputEvent, i),
			AMotionEvent_getFlags(inputEvent), 
			AMotionEvent_getMetaState(inputEvent),
			event.x, event.y);
		#endif
	}
	return this->onEvent ? 1 : 0;
}

int android_app::on_input_keyboard(AInputEvent* inputEvent)
{
	//键盘控制键管用，字符键不管用
	ANDROID_APP_LOGV("keyinput : action=%d flag=%d keycode=%d",
		AKeyEvent_getAction(inputEvent),
		AKeyEvent_getFlags(inputEvent),
		AKeyEvent_getKeyCode(inputEvent));

	APP_EVENT event;
	switch(AKeyEvent_getAction(inputEvent)){
	case AKEY_STATE_UNKNOWN:
		break;
	case AKEY_STATE_UP:
		event.id = APP_KEY_UP;
		break;
	case AKEY_STATE_DOWN:
		event.id = APP_KEY_DOWN;
		break;
	case AKEY_STATE_VIRTUAL:
		event.id = APP_KEY_PRESS;
		break;
	default:
		break;
	}

	event.key = AKeyEvent_getKeyCode(inputEvent);
	event.flag = AKeyEvent_getFlags(inputEvent);
	if(this->onEvent){//if processed, reutrn 1
		this->onEvent(this, event);
		return 1;
	}

	return 0;
}

// input event callback
int android_app::on_input_event(AInputEvent* inputEvent)
{
	if(AInputEvent_getType(inputEvent) == AINPUT_EVENT_TYPE_MOTION){
		return this->on_input_motion(inputEvent);
	}
	else if(AInputEvent_getType(inputEvent) == AINPUT_EVENT_TYPE_KEY){
		return this->on_input_keyboard(inputEvent);		
	}
	return 0;
}

// inputQueue callback
int android_app::on_input(int fd, int events, void* data)
{
	ANDROID_APP_LOGV("on_input %p", data);
	android_app* app = (android_app*)data;
	AInputEvent* event = NULL;
	while (AInputQueue_getEvent(app->inputQueue, &event) >= 0) {
		//ANDROID_APP_LOGV("New input event: type=%d\n", AInputEvent_getType(event));
		if (AInputQueue_preDispatchEvent(app->inputQueue, event)) {
			continue;
		}
		AInputQueue_finishEvent(app->inputQueue, event, app->on_input_event(event));
	}
	return 1;
}

//读取事件
bool android_app::read_event(APP_EVENT& event)
{
	return message.read(&event, sizeof(event)) == sizeof(event);
}

#ifdef ANDROID_APP_DEBUG
const char* app_event_name(int id)
{
	switch (id)
	{
		case APP_START:				return "APP_START";
		case APP_STOP:				return "APP_STOP";
		case APP_RESUME:			return "APP_RESUME";
		case APP_PAUSE:				return "APP_PAUSE";
		case APP_DESTROY:			return "APP_DESTROY";
		case APP_SAVE:				return "APP_SAVE";
		case APP_CONFIG:			return "APP_CONFIG";
		case APP_WINDOW_CREATE:		return "APP_WINDOW_CREATE";
		case APP_WINDOW_CLOSE:		return "APP_WINDOW_CLOSE";
		case APP_WINDOW_ACTIVATE:	return "APP_WINDOW_ACTIVATE";
		case APP_WINDOW_DEACTIVATE: return "APP_WINDOW_DEACTIVATE";
		case APP_WINDOW_RESIZE:		return "APP_WINDOW_RESIZE";
		case APP_INPUT:				return "APP_INPUT";
		default:
			break;
	}
	return null;
}
#endif

//预处理事件
void android_app::process_begin(APP_EVENT& event)
{
	switch (event.id){
	case APP_START:
	case APP_STOP:
	case APP_RESUME:
	case APP_PAUSE:
		ANDROID_APP_LOGV("APP_STATE = %s\n", app_event_name(event.id));
		this->activityState = event.id;
		break;
	case APP_DESTROY:
		ANDROID_APP_LOGV("APP_DESTROY\n");
		this->running = false;
		break;
	case APP_SAVE:
		//free_saved_state(this);
		break;
	case APP_CONFIG:
		ANDROID_APP_LOGV("APP_CONFIG\n");
		AConfiguration_fromAssetManager(this->config, this->activity->assetManager);
		this->print_config();
		break;

	case APP_WINDOW_CREATE:
		ANDROID_APP_LOGV("APP_WINDOW_CREATE : %p\n", event.data);
		this->window = (ANativeWindow*)event.data;
		break;
	case APP_INPUT:{
		ANDROID_APP_LOGV("APP_INPUT : %p\n", event.data);
		if(this->inputQueue){
			AInputQueue_detachLooper(this->inputQueue);
		}
		this->inputQueue = (AInputQueue*)event.data;
		if(this->inputQueue){
			AInputQueue_attachLooper(this->inputQueue, this->looper, 0, android_app::on_input, this);
		}
		break;
	}
	default:
		ANDROID_APP_LOGV("APP_EVENT : %s", app_event_name(event.id));
		break;
	}
}

//后续处理事件
void android_app::process_end(APP_EVENT& event)
{
	switch (event.id) {
	case APP_WINDOW_CLOSE:
		ANDROID_APP_LOGV("APP_WINDOW_CLOSE\n");
		this->window = NULL;
		break;
	case APP_SAVE:
		ANDROID_APP_LOGV("APP_SAVE\n");
		this->stateSaved = 1;
		break;
	case APP_RESUME:
		//free_saved_state(this);
		break;
	default:
		break;
	}
}

void android_app::dispose()
{
	ANDROID_APP_LOGV("android_app::dispose!");
	free_saved_state(this);
	//auto_lock lock(app->mutex);
	if(this->inputQueue){
		AInputQueue_detachLooper(this->inputQueue);
	}
	AConfiguration_delete(this->config);
	this->destroyed = 1;
	this->cond.broadcast();
	// Can't touch android_app object after this.
}

// app main thread
void* android_app_main(void* param)
{
	android_app* app = (android_app*)param;

	app->config = AConfiguration_new();
	AConfiguration_fromAssetManager(app->config, app->activity->assetManager);
	app->print_config();

	app->looper = ALooper_prepare(0);
	app->running = true;

	android_main(app);	//android_main

	app->dispose();

	return NULL;
}

// --------------------------------------------------------------------
//
// Native activity interaction (主线程调用的函数)
//
// --------------------------------------------------------------------

android_app* android_app_create(ANativeActivity* activity, void* savedState, size_t savedStateSize)
{
	android_app* app = new android_app;
	app->activity = activity;

	if (savedState != NULL) {
		/*
		app->savedState = malloc(savedStateSize);
		app->savedStateSize = savedStateSize;
		memcpy(app->savedState, savedState, savedStateSize);
		*/
	}

	if(app->message.create()){
		ANDROID_APP_ERROR("could not create pipe: %s", strerror(errno));
		return NULL;
	}

	//int flag = fcntl(app->message.in(), F_GETFL, 0 );
	//fcntl(app->message.in(), F_SETFL, flag | O_NONBLOCK);

	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_create(&app->thread, &attr, android_app_main, app);

	return app;
}

void android_app_post_event(android_app* app, const APP_EVENT& event)
{
	//ANDROID_APP_LOGV("write event %d", event.id);
	if(app->message.write(&event, sizeof(event)) != sizeof(event)) {
		ANDROID_APP_ERROR("Failure writing android_app event: %s\n", strerror(errno));
	}
}

void android_app_post_event(android_app* app, int id)
{
	APP_EVENT event = {id};
	android_app_post_event(app, event);
}

//
// 主线程回调函数
//

//
// app
//

void onStart(ANativeActivity* activity)
{
	ANDROID_APP_LOGV("Activity Start: %p\n", activity);
	android_app_post_event((android_app*)activity->instance, APP_START);
}

void onStop(ANativeActivity* activity)
{
	ANDROID_APP_LOGV("Activity Stop: %p\n", activity);
	android_app_post_event((android_app*)activity->instance, APP_STOP);
}

void onResume(ANativeActivity* activity)
{
	ANDROID_APP_LOGV("Activity Resume: %p\n", activity);
	android_app_post_event((android_app*)activity->instance, APP_RESUME);
}

void onPause(ANativeActivity* activity)
{
	ANDROID_APP_LOGV("Activity Pause: %p\n", activity);
	android_app_post_event((android_app*)activity->instance, APP_PAUSE);
}

void onDestroy(ANativeActivity* activity)
{
	ANDROID_APP_LOGV("Activity Destroy: %p\n", activity);
	android_app* app = (android_app*)activity->instance;
	//auto_lock lock(app->mutex);
	android_app_post_event(app, APP_DESTROY);
	while (!app->destroyed) {
		//app->cond_wait();
	}

	app->message.close();
	delete app;
}

void* onSaveInstanceState(ANativeActivity* activity, size_t* outLen)
{
	android_app* app = (android_app*)activity->instance;
	void* savedState = NULL;

	ANDROID_APP_LOGV("Activity SaveInstanceState: %p\n", activity);
	/*
	auto_lock lock(app->mutex);
	app->stateSaved = 0;
	android_app_post_event(app, APP_SAVE);
	while (!app->stateSaved) {
		app->cond_wait();
	}

	if (app->savedState != NULL) {
		savedState = app->savedState;
		*outLen = app->savedStateSize;
		app->savedState = NULL;
		app->savedStateSize = 0;
	}
	*/
	return savedState;
}

void onConfigurationChanged(ANativeActivity* activity) {
	ANDROID_APP_LOGV("Activity ConfigurationChanged: %p\n", activity);
	android_app_post_event((android_app*)activity->instance, APP_CONFIG);
}

void onLowMemory(ANativeActivity* activity)
{
	ANDROID_APP_LOGV("Activity LowMemory: %p\n", activity);
	android_app_post_event((android_app*)activity->instance, APP_LOW_MEMORY);
}

//
// window
//

void onNativeWindowCreated(ANativeActivity* activity, ANativeWindow* window)
{
	ANDROID_APP_LOGV("NativeWindowCreated: %p -- %p\n", activity, window);
	APP_EVENT event;
	event.id = APP_WINDOW_CREATE;
	event.data = window;
	android_app_post_event((android_app*)activity->instance, event);
}

void onNativeWindowDestroyed(ANativeActivity* activity, ANativeWindow* window)
{
	ANDROID_APP_LOGV("NativeWindowDestroyed: %p -- %p\n", activity, window);
	android_app_post_event((android_app*)activity->instance, APP_WINDOW_CLOSE);
}

void onWindowFocusChanged(ANativeActivity* activity, int focused)
{
	ANDROID_APP_LOGV("WindowFocusChanged: %p -- %d\n", activity, focused);
	android_app_post_event((android_app*)activity->instance, focused ? APP_WINDOW_ACTIVATE : APP_WINDOW_DEACTIVATE);
}

void onNativeWindowResized(ANativeActivity* activity, ANativeWindow* window)
{
	ANDROID_APP_LOGV("NativeWindowResized: %p -- %p\n", activity, window);
	android_app_post_event((android_app*)activity->instance, APP_WINDOW_RESIZE);
}

void onContentRectChanged(ANativeActivity* activity, const ARect* rect)
{
	ANDROID_APP_LOGV("ContentRectChanged: [%d, %d - %d, %d]\n", rect->left, rect->top, rect->right, rect->bottom);
	/*
	APP_EVENT event;
	event.id     = APP_WINDOW_RECT_CHANGED;
	event.left   = rect->left;
	event.top    = rect->top;
	event.right  = rect->right;
	event.bottom = rect->bottom;
	android_app_post_event((android_app*)activity->instance, event);
	*/
}

void onNativeWindowRedrawNeeded(ANativeActivity* activity, ANativeWindow* window)
{
	ANDROID_APP_LOGV("NativeWindowRedrawNeeded: %p -- %p\n", activity, window);
	android_app_post_event((android_app*)activity->instance, APP_WINDOW_PAINT);
}

//
// input
//

void android_app_set_input(android_app* app, AInputQueue* inputQueue)
{
	APP_EVENT event;
	event.id = APP_INPUT;
	event.data = inputQueue;
	android_app_post_event(app, event);
}

void onInputQueueCreated(ANativeActivity* activity, AInputQueue* queue)
{
	ANDROID_APP_LOGV("InputQueueCreated: %p -- %p\n", activity, queue);
	android_app_set_input((android_app*)activity->instance, queue);
}

void onInputQueueDestroyed(ANativeActivity* activity, AInputQueue* queue)
{
	ANDROID_APP_LOGV("InputQueueDestroyed: %p -- %p\n", activity, queue);
	android_app_set_input((android_app*)activity->instance, NULL);
}

//
// native activity entry
//

JNIEXPORT
void ANativeActivity_onCreate(ANativeActivity* activity, void* savedState, size_t savedStateSize)
{
	ANDROID_APP_LOGV("NativeActivity create: %p\n", activity);

	//app
	activity->callbacks->onStart   = onStart;
	activity->callbacks->onStop    = onStop;
	activity->callbacks->onResume  = onResume;
	activity->callbacks->onPause   = onPause;
	activity->callbacks->onDestroy = onDestroy;
	activity->callbacks->onSaveInstanceState    = onSaveInstanceState;
	activity->callbacks->onConfigurationChanged = onConfigurationChanged;
	activity->callbacks->onLowMemory = onLowMemory;

	//window
	activity->callbacks->onNativeWindowCreated      = onNativeWindowCreated;
	activity->callbacks->onNativeWindowDestroyed    = onNativeWindowDestroyed;
	activity->callbacks->onWindowFocusChanged       = onWindowFocusChanged;
	activity->callbacks->onNativeWindowResized      = onNativeWindowResized;
	activity->callbacks->onContentRectChanged       = onContentRectChanged;
	activity->callbacks->onNativeWindowRedrawNeeded = onNativeWindowRedrawNeeded;

	//input
	activity->callbacks->onInputQueueCreated   = onInputQueueCreated;
	activity->callbacks->onInputQueueDestroyed = onInputQueueDestroyed;

	//create android app
	activity->instance = android_app_create(activity, savedState, savedStateSize);

	//ANDROID_APP_LOGV("NativeActivity create successed.");
}

#endif /* ANDROID_APP_HPP  */
