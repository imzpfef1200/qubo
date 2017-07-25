//openCV
#include "opencv2/imgcodecs.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/videoio.hpp"
#include <opencv2/highgui.hpp>
#include <opencv2/video.hpp>
#include <opencv2/opencv.hpp>
#include "opencv2/bgsegm.hpp"

//C
#include <stdio.h>
//C++
#include <iostream>
#include <sstream>


using namespace cv;
using namespace std;


// Global variables
Mat frame; //current frame
Mat fgMaskMOG; //fg mask fg mask generated by MOG method
Mat fgMaskMOG2; //fg mask fg mask generated by MOG2 method
Ptr<BackgroundSubtractor> pMOG; //MOG Background subtractor
Ptr<BackgroundSubtractor> pMOG2; //MOG2 Background subtractor
int slider_area = 0, slider_circularity = 0, slider_ratio = 0, slider_convexity = 0, kernel_size = 1;
int MAX_area = 9000, MAX_circularity = 1000, MAX_ratio = 1000, MAX_convexity = 1000;

// Setup SimpleBlobDetector parameters.
SimpleBlobDetector::Params params;
Ptr<SimpleBlobDetector> detector;
vector<tuple<Point2f, Vec3b, int>> history;

VideoCapture cap;


void blurCallback(int, void* )
{

	//the kernel for a guassian filter needs to be odd
	kernel_size = (round(kernel_size / 2.0) * 2) -1; //round down to nearest odd integer

	//make sure we don't have a negative number (error from round) or zero
	if (kernel_size < 1){
		kernel_size = 1;
	}

	//let the user know what the actual kernel being used is (kernel of one == no blur)
	setTrackbarPos("blur kernel","parameters", kernel_size);
	
}

void areaCallback( int, void* )
{
    //Filter by Area.
    params.filterByArea = true;
    params.minArea = slider_area;
    params.maxArea = MAX_area;
	detector = SimpleBlobDetector::create(params);
}

void circularityCallback( int, void* )
{
    //Filter by Circularity.
    params.filterByCircularity = true; 
    params.minCircularity = (float) slider_circularity / (float) MAX_circularity;
	detector = SimpleBlobDetector::create(params);
}

void convexityCallback( int, void* )
{
    //Filter by Convexity.
    params.filterByConvexity = true; 
    params.minConvexity = (float) slider_convexity / (float) MAX_convexity;
	detector = SimpleBlobDetector::create(params);
}

void inertiaCallback( int, void* )
{
    //Filter by ratio of the inertia.
    params.filterByInertia = true; 
    params.minInertiaRatio = (float) slider_ratio / (float) MAX_ratio;
	detector = SimpleBlobDetector::create(params);
}


//Apply's the MOG subtraction
Mat backgroundSubtract(const Mat cframe){

	Mat out_frame; // output matrix

	
	//update the background model
	pMOG->apply(cframe, out_frame);
		
	
    //blurs the image uses the MOG background subtraction
    GaussianBlur(out_frame, out_frame, Size(3,3), 0,0);

	
    // Define the structuring elements to be used in eroding and dilating the image 
    Mat se1 = getStructuringElement(MORPH_RECT, Size(5, 5));
    Mat se2 = getStructuringElement(MORPH_RECT, Size(5, 5));

    // Perform dialting and eroding helps to elminate background noise
	//sgillen@20175109-15:51 when I found this on the second operation was actually being performed
    morphologyEx(out_frame, out_frame, MORPH_CLOSE, se1);
    morphologyEx(out_frame, out_frame, MORPH_OPEN, se2);

    //inverts the colors 
    bitwise_not(out_frame, out_frame, noArray()); 


	return out_frame;
}


//Davids way of keeping track of the history of points
bool updateHistory(const Mat cframe, vector<KeyPoint> keypoints, Point2f center){
    float pointX, pointY, x, y; 
    bool insert; 
    Vec3b color;
    int age = 10, offSet = 20, filter = 30, offSet2 = 5;//how long ago the blob was first seen and the offset of the center and the value for the color we want to see 

    //for every deteced blob either add it if its new or update current one 
    for (auto& point:keypoints ){    
        color = cframe.at<Vec3b>(point.pt); 
        cout << color << endl;
        insert = false;
        pointX = point.pt.x;
        pointY = point.pt.y;
        for (std::vector<tuple< Point2f, Vec3b, int >>::iterator it = history.begin(); it != history.end(); it++){ 
            x = std::get<0>(*it).x;
            y = std::get<0>(*it).y;   

            //if blob is within offSet pixels of a know blob update the blob to the new blobs location       
            if (((pointX <= x + offSet) && (pointX >= x - offSet)) && ((pointY <= y + offSet && (pointY >= y - offSet)))){
                history.erase(it);
                history.emplace_back(std::make_tuple (point.pt,color,0));
                insert = true;
            }
            std::get<2>(*it) += 1;
            //if the blobs hasnt been updated in age frames remove it 
            if (std::get<2>(*it) > age){
                history.erase(it);
            }
        }
        if (!insert)
            history.emplace_back(std::make_tuple (point.pt, color, 0));
    }
    //outputs the buoys offset if it is the right color.  
    for (std::vector<tuple< Point2f, Vec3b, int >>::iterator it = history.begin(); it != history.end(); it++){
        color = std::get<1>(*it);
        if (color[0] >= filter - offSet2 && color[0] <= filter + offSet2){
            center =  std::get<0>(*it);
            return true;}
    } 

    return false;
}


int main(int argc, char* argv[])
{

    //check for the input parameter correctness
	if(argc != 2){
		cerr <<"Incorrect input list" << endl;
		cerr <<"exiting..." << endl;
		return EXIT_FAILURE;
	}

	cap = cv::VideoCapture(argv[1]);

	if(!cap.isOpened()){           
        printf("couldn't open file/camera  %s\n now exiting" ,argv[1]);
        exit(0);
    }else{printf("opened file/camera %s\n", argv[1]);}

   
	

	//create GUI window for the keypoints
	namedWindow("keypoints");
	moveWindow("keypoints", 20, 20);

	namedWindow("Gray image");
	moveWindow("Gray image", 20, 20);
	

	//create all the trackbars
	createTrackbar( "area", "keypoints", &slider_area, MAX_area, areaCallback); 
	createTrackbar( "circularity", "keypoints", &slider_circularity, MAX_circularity, circularityCallback);
	createTrackbar( "convexity", "keypoints", &slider_convexity, MAX_convexity, convexityCallback);
	createTrackbar( "inertia ratio", "keypoints", &slider_ratio, MAX_ratio, inertiaCallback);
	createTrackbar( "blur kernel", "keypoints", &kernel_size, 256, blurCallback);

	//create Background Subtractor objects
	pMOG = bgsegm::createBackgroundSubtractorMOG(1000,5,.7,0);
	pMOG2 = createBackgroundSubtractorMOG2(1000,16,false);


	params.minThreshold = 0;
	params.maxThreshold = 256;
	//Filter by Area
	params.filterByArea = true;
	params.minArea = 100;
	
	detector = SimpleBlobDetector::create(params);
	
	
	Mat cframe;


	while(true){
		cap >> cframe; 


		Mat mog_output, gdst; //output from our background subtractor, we need to keep track of the unmodified current frame 
		vector<KeyPoint> keypoints; // Storage for blobs
		
		
		Point2f center; 
		
		
		GaussianBlur(cframe, gdst, Size( kernel_size, kernel_size ), 0, 0 );
		
		mog_output = backgroundSubtract(gdst); //updates the MOG frame
		
		detector->detect(mog_output, keypoints);
		drawKeypoints(cframe, keypoints, cframe, Scalar(0,0,255), DrawMatchesFlags::DRAW_RICH_KEYPOINTS );
			
		
		
		imshow("Gray image" , mog_output);
		imshow("keypoints"  , cframe);
		waitKey();

		int x_offset;
		int y_offset;
		
		if (updateHistory(mog_output, keypoints, center)){
			x_offset = cframe.rows/2 - center.x; 
			y_offset = cframe.cols/2 - center.y;
			
			
		}
	}
	
	
	//destroy GUI windows
	destroyAllWindows();
	return EXIT_SUCCESS;
}

