
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <cmath>
#include <cstdio>
#include <queue>

// ============================================================
// Configuration
// ============================================================

int main()
{
    const int lowThreshold = 100;
    const int highThreshold = 200;

    // ========================================================
    // 1. Load input image
    // ========================================================

    cv::Mat image = cv::imread("images\\input.png");

    if (image.empty()) {
        std::printf("Failed to load input image\n");
        return 1;
    }

    // ========================================================
    // 2. Grayscale and Gaussian blur
    // ========================================================

    cv::Mat img_gray;
    cv::Mat blurred;

    cv::cvtColor(image, img_gray, cv::COLOR_BGR2GRAY);

    if (!cv::imwrite("images\\01_grayscale.png", img_gray)) {
        std::printf("Failed to save grayscale image\n");
        return 2;
    }

    cv::GaussianBlur(
        img_gray,
        blurred,
        cv::Size(3, 3),
        0
    );

    if (!cv::imwrite("images\\02_gaussian_blur.png", blurred)) {
        std::printf("Failed to save blurred image\n");
        return 3;
    }

    // ========================================================
    // 3. Sobel gradients and gradient magnitude
    // ========================================================

    cv::Mat sobel[2];

    int j = 1;
    int k = 0;

    for (int i = 0; i < 2; i++) {
        cv::Sobel(
            blurred,
            sobel[i],
            CV_32F,
            j,
            k
        );

        j--;
        k++;
    }

    cv::Mat grad_result;

    cv::magnitude(sobel[0], sobel[1], grad_result);

    double minGradient, maxGradient;

    cv::minMaxLoc(grad_result, &minGradient, &maxGradient);

    std::printf("Gradient magnitude: min = %.2f, max = %.2f\n", minGradient, maxGradient);

    // Convert gradient magnitude to an 8-bit image for viewing.
    cv::Mat gradient_view;
    cv::normalize(
        grad_result,
        gradient_view,
        0,
        255,
        cv::NORM_MINMAX,
        CV_8U
    );

    if (!cv::imwrite("images\\03_gradient_magnitude.png", gradient_view)) {
        std::printf("Failed to save gradient magnitude image\n");
        return 4;
    }

    // ========================================================
    // 4. Non-Maximum Suppression (NMS)
    // ========================================================

    cv::Mat nms = cv::Mat::zeros(
        img_gray.rows,
        img_gray.cols,
        CV_32F
    );

    for (int i = 1; i < grad_result.rows - 1; i++) {
        for (int j = 1; j < grad_result.cols - 1; j++) {
            float angle0 = sobel[0].at<float>(i, j);
            float angle1 = sobel[1].at<float>(i, j);

            double angle = std::atan2(angle1, angle0);
            double degrees = angle * 180.0 / CV_PI;

            if (degrees < 0) {
                degrees += 180;
            }

            float current = grad_result.at<float>(i, j);
            float neighbor1;
            float neighbor2;

            // 0 degrees
            if (degrees >= 157.5 || degrees < 22.5) {
                neighbor1 = grad_result.at<float>(i, j - 1);
                neighbor2 = grad_result.at<float>(i, j + 1);
            }
            // 45 degrees
            else if (degrees >= 22.5 && degrees < 67.5) {
                neighbor1 = grad_result.at<float>(i - 1, j + 1);
                neighbor2 = grad_result.at<float>(i + 1, j - 1);
            }
            // 90 degrees
            else if (degrees >= 67.5 && degrees < 112.5) {
                neighbor1 = grad_result.at<float>(i - 1, j);
                neighbor2 = grad_result.at<float>(i + 1, j);
            }
            // 135 degrees
            else {
                neighbor1 = grad_result.at<float>(i - 1, j - 1);
                neighbor2 = grad_result.at<float>(i + 1, j + 1);
            }

            if (current >= neighbor1 && current >= neighbor2) {
                nms.at<float>(i, j) = current;
            }
        }
    }

    int nmsAboveLow = 0;
    int nmsAboveHigh = 0;

    for (int i = 0; i < nms.rows; i++) {
        for (int j = 0; j < nms.cols; j++) {
            float value = nms.at<float>(i, j);

            if (value >= lowThreshold) {
                nmsAboveLow++;
            }

            if (value >= highThreshold) {
                nmsAboveHigh++;
            }
        }
    }

    std::printf("NMS pixels >= low threshold: %d\n", nmsAboveLow);
    std::printf("NMS pixels >= high threshold: %d\n", nmsAboveHigh);

    cv::Mat nms_view;

    cv::normalize(
        nms,
        nms_view,
        0,
        255,
        cv::NORM_MINMAX,
        CV_8U
    );

    if (!cv::imwrite("images\\04_nms.png", nms_view)) {
        std::printf("Failed to save NMS image\n");
        return 5;
    }

    // ========================================================
    // 5. Double thresholding
    // ========================================================

    int strongCount = 0;
    int weakCount = 0;
    int rejectedCount = 0;

    cv::Mat thresholded = cv::Mat::zeros(
        img_gray.rows,
        img_gray.cols,
        CV_8U
    );

    for (int i = 1; i < nms.rows - 1; i++) {
        for (int j = 1; j < nms.cols - 1; j++) {
            float current = nms.at<float>(i, j);

            if (current < lowThreshold) {
                thresholded.at<unsigned char>(i, j) = 0;
                rejectedCount++;
            }
            else if (current < highThreshold) {
                thresholded.at<unsigned char>(i, j) = 75;
                weakCount++;
            }
            else {
                thresholded.at<unsigned char>(i, j) = 255;
                strongCount++;
            }
        }
    }

    std::printf("Rejected pixels: %d\n", rejectedCount);
    std::printf("Weak edge pixels: %d\n", weakCount);
    std::printf("Strong edge pixels: %d\n", strongCount);

    if (!cv::imwrite("images\\05_double_threshold.png", thresholded)) {
        std::printf("Failed to save double-threshold image\n");
        return 6;
    }

    // ========================================================
    // 6. Edge tracking by hysteresis (BFS)
    // ========================================================

    std::queue<cv::Point> q;

    // Add all strong edges to the queue.
    for (int i = 0; i < thresholded.rows; i++) {
        for (int j = 0; j < thresholded.cols; j++) {
            if (thresholded.at<unsigned char>(i, j) == 255) {
                q.push(cv::Point(j, i));
            }
        }
    }

    // Promote weak edges connected to strong edges.
    while (!q.empty()) {
        cv::Point p = q.front();
        q.pop();

        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                if (dx == 0 && dy == 0) {
                    continue;
                }

                int x = p.x + dx;
                int y = p.y + dy;

                if (x < 0 || x >= thresholded.cols ||
                    y < 0 || y >= thresholded.rows) {
                    continue;
                }

                if (thresholded.at<unsigned char>(y, x) == 75) {
                    thresholded.at<unsigned char>(y, x) = 255;
                    q.push(cv::Point(x, y));
                }
            }
        }
    }

    int survivingWeak = 0;

    for (int i = 0; i < thresholded.rows; i++) {
        for (int j = 0; j < thresholded.cols; j++) {
            if (thresholded.at<unsigned char>(i, j) == 75) {
                survivingWeak++;
            }
        }
    }

    std::printf("Unconnected weak edge pixels: %d\n", survivingWeak);

    int connectedEdges = 0;

    for (int i = 0; i < thresholded.rows; i++) {
        for (int j = 0; j < thresholded.cols; j++) {
            if (thresholded.at<unsigned char>(i, j) == 255) {
                connectedEdges++;
            }
        }
    }

    std::printf("Edges after hysteresis: %d\n", connectedEdges);

    // Discard weak edges that were not connected to strong edges.
    for (int i = 0; i < thresholded.rows; i++) {
        for (int j = 0; j < thresholded.cols; j++) {
            if (thresholded.at<unsigned char>(i, j) == 75) {
                thresholded.at<unsigned char>(i, j) = 0;
            }
        }
    }

    if (!cv::imwrite("images\\06_my_own_canny.png", thresholded)) {
        std::printf("Failed to save custom Canny result\n");
        return 7;
    }

    // ========================================================
    // 7. OpenCV Canny reference
    // ========================================================

    cv::Mat edges;

    cv::Canny(blurred, edges, 100, 200, 3, true);

    if (!cv::imwrite("images\\07_canny_opencv.png", edges)) {
        std::printf("Failed to save OpenCV Canny result\n");
        return 8;
    }

    int myEdgeCount = cv::countNonZero(thresholded);
    int openCvEdgeCount = cv::countNonZero(edges);

    std::printf("Custom Canny edges: %d\n", myEdgeCount);
    std::printf("OpenCV Canny edges: %d\n", openCvEdgeCount);

    return 0;
}
