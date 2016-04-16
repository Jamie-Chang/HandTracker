#include<opencv2/core/core.hpp>
#include<opencv2/highgui/highgui.hpp>
#include<opencv2/imgproc/imgproc.hpp>
#include<opencv2/video/tracking.hpp>
#include<opencv2/video/background_segm.hpp>
#include<stdexcept>
#include<iostream>
#include<string>
#include<vector>
#include<chrono>
#include<fstream>
#include<thread>
#include"Hist.h"
#include"Window.h"
#include"Background.h"
#include"Tracker.h"
#include"Demo.h"
#include"Log.h"
#include"Settings.h"	
#include"Exception.h"

#define LEFT_KEY 2424832
#define RIGHT_KEY 2555904
#define UP_KEY 2490368
#define DOWN_KEY 2621440
#define ENTER_KEY 13
#define EQUAL_PLUS_KEY '='
#define MINUS_UNDERSCORE_KEY '-'
#define ESC_KEY 27

#define OP_SETUP 0
#define OP_MAIN 1
#define OP_EXIT 2

extern Logger program_log;



int main(int argc, char ** argv)
{
	
	long double start, end;					// timer
	program_log.event("Initializing.");
	
	cv::VideoCapture stream(1);			// initialte stream from vidoe camera.

	int operation_mode = OP_SETUP;
	cv::Mat imgBGR, imgHSV, backproj, foreground, hist, segmented;
	//CenteredRect track_win;
	// initialize selection window to default size and position.
	Window select_win = Window(DEFAULT_POSITION, DEFAULT_SIZE);	

	// initiate Mixture of Gaussian background subtractor
	BackgroundSubtractor bg = BackgroundSubtractor(200, 24);
	program_log.event("Learning background.");
	for (int j = 0; j < INIT_BACK_SUB && stream.isOpened() && stream.read(imgBGR); j++)
	{
		cv::Mat bgImg; 
		cv::flip(imgBGR, imgBGR, 1);						// flips the frame to mirrror movement
		bg.apply_frame(imgBGR, foreground, 0.5);
		
		bg.getBackground(bgImg);
		cv::imshow("Setting up Background...", bgImg);
		int keypress = cv::waitKey(1);
	}
	
	cv::destroyWindow("Setting up Background...");
	Tracker tracker;
	if (DISPLAY_DEMO)
	{
		std::thread t(graphics::setup, argc, argv);
		t.detach();
	}


	// frame capture loop
	while (stream.isOpened() && operation_mode != OP_EXIT) {
		program_log.frame();
		auto start = chrono::system_clock::now();
		// Set up frame here.
		if (!stream.read(imgBGR) || imgBGR.empty()) {
			throw FailedToRead("FAILURE: cannot read image/frame.");
			return 0;
		}		
		cv::flip(imgBGR, imgBGR, 1);						// flips the frame to mirrror movement
		cv::cvtColor(imgBGR, imgHSV, cv::COLOR_BGR2HSV);	// convert the image to hue sturation and value image
		
		
		int keyPress = cv::waitKey(1);						// waits 1ms and assigns keypress (if any) to keypress

		// setup stage displays window to capture histogram of hand
		// window is allowed to move in position and change in szie
		if (operation_mode == OP_SETUP) {
			cv::MatND hist;
			int delta = select_win.outer.size().width / 4.0;// unit size for the change in control
			switch (keyPress)								// controls for the selection phase
			{
			case LEFT_KEY:
				select_win.move(cv::Point(-delta, 0));
				break;
			case RIGHT_KEY:
				select_win.move(cv::Point(delta, 0)); 
				break;
			case UP_KEY:
				select_win.move(cv::Point(0, -delta));
				break;
			case DOWN_KEY:
				select_win.move(cv::Point(0, delta)); 
				break;
			case EQUAL_PLUS_KEY:
				select_win.scale(1.1);
				break;
			case MINUS_UNDERSCORE_KEY:
				select_win.scale(0.9);
				break;
			case ENTER_KEY:	
				hist = select_win.histogram(imgBGR);
				tracker = Tracker(select_win.inner, hist, bg);
				operation_mode = OP_MAIN;		// advance to tracking mode
				break;
			case ESC_KEY:
				operation_mode = OP_EXIT;
				break;
			}
			select_win.draw(imgBGR);						// draw the window on the coloured image
		}

		
		// track the hand's position in real time 
		else if (operation_mode == OP_MAIN) {
			try 
			{
				tracker.process_frame(imgBGR, imgHSV, foreground, backproj, segmented);
			}
			catch(TrackingException& e)
			{
				std::cout << e.what() << endl;
				program_log.except(e);
				operation_mode = OP_SETUP;
			}
			switch (keyPress)
			{
			case 'r':
				operation_mode = OP_SETUP;
				break;
			case 'c':
				cv::imwrite("screencap_rgb.jpg", imgBGR);
				cv::imwrite("screencap_hsv.jpg", imgHSV);
				break;
			case ESC_KEY:
				operation_mode = OP_EXIT;
				break;
			}
		}
		// display the window
		cv::namedWindow("Tracker", CV_WINDOW_AUTOSIZE);
		cv::imshow("Tracker", imgBGR); 
	}
	cout << "Exiting tracker!" << endl;
	cv::destroyAllWindows();
	
	std::system("pause");
	return 0;
}

