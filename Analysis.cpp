#include"Analysis.h"
#include<thread>

inline float dist(cv::Point& p, cv::Point& q) 
{
	return cv::norm(p - q);
}

inline bool isAcute(cv::Point& p1, cv::Point& p2, cv::Point& q)
{
	return (p1 - q).ddot(p2 - q) >= 0.0;
}

inline int positive_modulo(int i, int n) {
	return (i % n + n) % n;
}

inline bool lessThan60(cv::Point& p1, cv::Point& p2, cv::Point& q)
{
	cv::Point v1 = p1 - q;
	cv::Point v2 = p2 - q;
	return v1.ddot(v2) >= 0.5 * cv::norm(v1) * cv::norm(v2);
}

inline bool lessThan120(cv::Point& p1, cv::Point& p2, cv::Point& q)
{
	cv::Point v1 = p1 - q;
	cv::Point v2 = p2 - q;
	return v1.ddot(v2) >= -0.5 * cv::norm(v1) * cv::norm(v2);
}


inline double cosine_angle(cv::Point& v1, cv::Point& v2)
{
	return v1.ddot(v2) / cv::norm(v1) / cv::norm(v2);
}

inline double cosine_angle(cv::Point& p1, cv::Point& p2, cv::Point& q)
{
	cv::Point v1 = p1 - q;
	cv::Point v2 = p2 - q;
	return cosine_angle(v1, v2);
}

bool kcurvature(vector<cv::Point>& contour, int index, int k_value)
{
	cv::Point q = contour[index];
	cv::Point p1 = contour[positive_modulo((index + k_value), contour.size())];
	cv::Point p2 = contour[positive_modulo((index + k_value), contour.size())];

	return lessThan60(p1, p2, q);

}


inline int point_above_line(cv::Point l1, cv::Point l2, cv::Point pt)
{
	double d = (pt.x - l1.x)*(l2.y - l1.y) - (pt.y - l1.y)*(l2.x - l1.x);
	if (d > 0) return 1;
	else if (d < 0) return -1;
	else return  0;
}


inline int point_above_line_v2(cv::Point point_on_line, cv::Point direction, cv::Point point)
{
	cv::Point l1 = point_on_line - 10 * direction;
	cv::Point l2 = point_on_line + 10 * direction;
	return point_above_line(l1, l2, point);
}

inline double distance_from_line(cv::Point l1, cv::Point l2, cv::Point pt)
{
	cv::Point dir = l2 - l1;
	dir /= cv::norm(dir);
	cv::Point normal = cv::Point(-dir.x, dir.y);
	return normal.ddot(pt - l1);
}


inline bool point_above_wrist(cv::Point& wrist, cv::Point& center, cv::Point& pt)
{
	return isAcute(pt, center, wrist);
}


inline cv::Point tangent(cv::Point p1, cv::Point p2)
{
	cv::Point temp = p2 - p1; 
	return cv::Point(temp.y, -temp.x);
}

vector<cv::Point> contourClustering(vector<cv::Point> contour, list<int> potential_indices)
{
	vector<cv::Point> fingers(0);

	int running_average = 0, running_count = 0;

	while (potential_indices.size() > 0 || running_count > 0)
	{
		if (potential_indices.size() == 0)
		{
			int index = running_average >= 0 ? running_average : contour.size() + running_average;
			fingers.push_back(contour[index]);
			running_average = 0;
			running_count = 0;
			break;
		}

		if (running_count == 0)
		{
			running_count++;
			running_average = potential_indices.front();
			potential_indices.pop_front();
		}
		else
		{
			int previous = potential_indices.back() - contour.size(),
				next = potential_indices.front();

			if (abs(running_average - previous) < 15)
			{
				int total = running_average * running_count + previous;
				running_count++; total /= running_count;
				potential_indices.pop_back();
			}
			else if (abs(next - running_average) < 15)
			{
				int total = running_average * running_count + next;
				running_count++; total /= running_count;
				potential_indices.pop_front();
			}
			else {
				int index = running_average >= 0 ? running_average : contour.size() + running_average;
				fingers.push_back(contour[index]);
				running_average = 0;
				running_count = 0;
			}
		}
	}
	return fingers;
}






HandAnalysis::HandAnalysis()
{
	contour = vector<cv::Point>(0);
	fingers = vector<cv::Point>(0);
	hull = vector<cv::Point>(0);
}


void HandAnalysis::apply(cv::Mat &frame, cv::Mat &probImg, CenteredRect &bounds)
{
	this->frame = frame;
	this->prob = probImg;
	this->proposed_roi = bounds;
	threshold(); // thresholds the probImg and assigns the new image to thresh.
	max_contour();
	// two processes can be parallelized.
	std::thread threadA = std::thread(&HandAnalysis::find_center_orientation, this);
	std::thread threadB = std::thread(&HandAnalysis::max_hull, this);
	threadA.join();
	threadB.join();
	finger_tips();
	//finger_tips2();

	bounds = CenteredRect(center, cv::Size(radius * 2, radius * 2));
}

void HandAnalysis::show()
{
	if (contour.size() > 0)
	{
		vector<vector<cv::Point>> temp_contour(0);
		vector<vector<cv::Point>> temp_hull(0);
		temp_contour.push_back(contour);
		temp_hull.push_back(hull);
		cv::drawContours(frame, temp_contour, 0, CV_RGB(0, 255, 0), 2, 8);

		cv::drawContours(frame, temp_hull, 0, CV_RGB(0, 0, 255), 2, 8);
		for (cv::Point finger : fingers)
			cv::circle(frame, finger, 5, CV_RGB(255, 0, 0), 2);
		cv::circle(frame, center, 5, CV_RGB(255, 0, 255), 2);

		cv::circle(frame, center, radius, CV_RGB(255, 0, 255), 2);
		cv::line(frame, wrist, center, CV_RGB(255, 255, 0), 2);
	}

}

void HandAnalysis::threshold()
{
	cv::GaussianBlur(prob, thresh, cv::Size(5, 5), 0, 0);

	cv::threshold(thresh, thresh, 25, 255, cv::THRESH_BINARY);
	cv::Mat element = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(10, 10));

	cv::erode(thresh, thresh, element);
	cv::dilate(thresh, thresh, element);
}

void HandAnalysis::max_contour()
{
	vector<cv::Vec4i> hierarchy;
	vector<vector<cv::Point> > contours(0);


	cv::findContours(thresh, contours, hierarchy, CV_RETR_TREE, CV_CHAIN_APPROX_SIMPLE);

	/// Find the convex hull,contours and defects for each contour

	double max_area = 0.0;
	int max_index = -1;
	cv::Rect region, max_region;
	for (int i = 0; i < contours.size(); i++)
	{
		double a = contourArea(contours[i], false);  //  Find the area of contour
		if (a > max_area)
		{
			region = cv::boundingRect(contours[i]);
			if ((region & proposed_roi).area() > proposed_roi.area() * 0.8)
			{
				max_area = a;
				max_index = i;                //Store the index of largest contour
				max_region = region;
			}
		}
	}

	if (max_index == -1) throw std::exception("Lost hand!");

	contour = contours[max_index];
	bounding_box = cv::boundingRect(contour);
	// update thresholded binary image with the maximum contour. 
	thresh = cv::Mat::zeros(frame.size(), CV_8U);
	cv::drawContours(thresh, contours, max_index, CV_RGB(255, 255, 255), -1);
}




void HandAnalysis::max_hull()
{
	hull_indices = vector<int>(0);

	cv::convexHull(cv::Mat(contour), hull_indices, false);


	hull = vector<cv::Point>(hull_indices.size());
	
	list<int> temp = list<int>();
	
	for (int i : hull_indices)
		temp.push_back(i);

	contourClustering(contour, temp);
	hull_indices = vector<int>();

	for (int i : temp)
		hull_indices.push_back(i);

	for (int i = 0; i < hull_indices.size(); i++)
		hull[i] = contour[hull_indices[i]];

	if (hull_indices.size() > 3) {
		defects = vector<cv::Vec4i>(0);
		cv::convexityDefects(contour, hull_indices, defects);
	}

}

void HandAnalysis::find_center()
{
	distTransform = cv::Mat::zeros(frame.size(), CV_32F);
	cv::distanceTransform(thresh(bounding_box), distTransform(bounding_box), CV_DIST_L2, 3, CV_32F);
	double a, b; cv::Point t;
	cv::minMaxLoc(distTransform(proposed_roi), &a, &b, &t, &center);
	center += proposed_roi.tl();
	radius = distTransform.at<float>(center);

	CenteredRect new_region = CenteredRect(center, cv::Size(radius * 2, radius * 2)) 
		& cv::Rect(cv::Point(0,0), frame.size());
	cv::Moments m = cv::moments(distTransform(new_region));
	center = cv::Point(m.m10 / m.m00, m.m01 / m.m00) + new_region.tl();
}


void HandAnalysis::find_wrist()
{
	cv::Mat mask = cv::Mat::zeros(frame.size(), CV_32F);
	cv::circle(mask, center, radius, CV_RGB(255, 255, 255), 1);
	cv::rectangle(mask, cv::Rect(center - cv::Point(radius, radius), 
		center + cv::Point(radius, 0)), CV_RGB(0, 0, 0), -1);

	cv::bitwise_and(mask, distTransform, mask);

	cv::Moments m = cv::moments(mask);
	wrist = cv::Point(m.m10 / m.m00, m.m01 / m.m00);
	cv::Point axis = center - wrist;
	axis *= (radius / cv::norm(axis));
	wrist = center - axis;
}

void HandAnalysis::find_center_orientation()
{
	find_center();
	find_wrist();
}



void HandAnalysis::finger_tips()
{
	vector<cv::Vec4i> filteredDefects(0);
	list<int> fingerTipIndices(0);
	for (cv::Vec4i def : defects)
	{
		cv::Point start(contour[def.val[0]]);
		cv::Point end(contour[def.val[1]]);
		cv::Point far(contour[def.val[2]]);

		double depth = def.val[3] / 256.0;
		if (depth > radius / 2)
			filteredDefects.push_back(def);
	}

	for (int i = 0; i < filteredDefects.size(); i++)
	{
		int size = contour.size();
		cv::Vec4i current = filteredDefects[i];
		if (kcurvature(contour, current.val[0], size / 25))
			fingerTipIndices.push_back(current.val[0]);

		if (kcurvature(contour, current.val[1], size / 25))
			fingerTipIndices.push_back(current.val[1]);
	}

	fingerTipIndices.sort();
	vector<cv::Point> potential(0);

	potential = contourClustering(contour, fingerTipIndices);

	// further remove false negatives
	fingers = vector<cv::Point>(0);
	finger_dist = vector<double>(0);
	finger_height = vector<double>(0);


	cv::Point axis = (center - wrist) / radius;

	for (cv::Point tip : potential)
	{
		double distance = dist(tip, center);
		if (distance > 1.6 * radius && distance < 4 * radius)
		{
			double v_distance = axis.ddot(tip - wrist);
			if (v_distance > 0)
			{
				finger_dist.push_back(distance);
				finger_height.push_back(v_distance);
				fingers.push_back(tip);
			}
		}
	}

}



void HandAnalysis::finger_tips2()
{
	fingers = vector<cv::Point>(0);

	cv::Point axis = (center - wrist) / radius;

	for (int pt : hull_indices)
	{
		cv::Point tip = contour[pt];
		double distance = dist(tip, center);
		if (distance > 1.6 * radius && distance < 4 * radius)
		{
			double v_distance = axis.ddot(tip - wrist);
			if (v_distance > 0)
			{
				if (kcurvature(contour, pt, contour.size() / 25))
				{
					finger_dist.push_back(distance);
					finger_height.push_back(v_distance);
					fingers.push_back(tip);
				}

			}
		}
	}


}
