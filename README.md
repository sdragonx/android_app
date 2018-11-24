# android_app
android_app c++ framework ( public domain )

//app entry
void android_main(android_app* app)
{
　　//APP_LOG("main : start");
　　app->onEvent = on_app_event;
 
　　//main loop
　　while (app->running){
　　　　app->process_message();
　　　　if(!app->pause()){
　　　　　　//draw_frame();
　　　　}
　　　　else{
　　　　　　//APP_LOG("PAUSE");
　　　　}
　　}
　　//APP_LOG("main : end");
}

//APP_EVENT struct
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

//main event callback
void on_app_event(android_app* app, APP_EVENT& event)
{
    switch(event.id)
    {
    case APP_START://activity启动
        break;
    case APP_STOP://停止
        break;
    case APP_RESUME://返回
        break;
    case APP_PAUSE://暂停
        break;
    case APP_DESTROY://销毁退出
        break;
    case APP_SAVE://保存状态
        break;
    case APP_WINDOW_CREATE://窗口创建，app->window有效
        break;
    case APP_WINDOW_CLOSE://窗口销毁
        break;
    case APP_WINDOW_ACTIVATE://窗口获得焦点
        break;
    case APP_WINDOW_DEACTIVATE://窗口失去焦点
        break;
    case APP_WINDOW_RESIZE://窗口大小改变
        break;
 
    case APP_TOUCH_DOWN://触摸按下
        //这三个事件，都使用event.x, event.y作为坐标
        //event.z代表pointerid
        break;
    case APP_TOUCH_MOVE://触摸滑动
        break;
    case APP_TOUCH_UP:
        break;
 
    default:
        //APP_LOG("unknow msg:%d", cmd);
        break;
    }
}　
