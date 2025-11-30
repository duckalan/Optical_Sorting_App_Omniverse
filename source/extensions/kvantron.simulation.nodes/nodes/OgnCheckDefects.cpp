#include "OgnCheckDefectsDatabase.h"
#include <algorithm>
#include <carb/RenderingTypes.h>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <omni/graph/action/IActionGraph.h>
#include <omni/graph/core/iComputeGraph.h>
#include <opencv2/core.hpp>
#include <opencv2/core/cvdef.h>
#include <opencv2/core/hal/interface.h>
#include <opencv2/core/mat.hpp>
#include <opencv2/core/matx.hpp>
#include <opencv2/core/types.hpp>
#include <opencv2/imgproc.hpp>
#include <stdexcept>
#include <string>
#include <vector>

namespace kvantron
{
namespace simulation
{
namespace nodes
{

class CapProcessor {
private:
    bool isColored{ true };
    cv::Mat element1;
    cv::Mat element2;
    cv::Mat imageForOvality;
    cv::Mat imageForInclusions;
    cv::Mat imageForPaintDefects;

public:
    static constexpr uint8_t BLUE_CAPS = 80;
    static constexpr uint8_t YELLOW_CAPS = 160;
    static constexpr uint8_t GOLD_CAPS = 160;
    static constexpr uint8_t WHITE_CAPS = 160;
    static constexpr int window = 15;

    CapProcessor(
        bool needCheckForOvality,
        bool needCheckForInclusions,
        bool needCheckForPaintDefects,
        const cv::Mat& inputFrame
    )
    {
        InitializeMorphologicalElements();
        if (needCheckForOvality) {
            inputFrame.copyTo(imageForOvality);
        }
        if (needCheckForInclusions) {
            inputFrame.copyTo(imageForInclusions);
        }
        if (needCheckForPaintDefects) {
            inputFrame.copyTo(imageForPaintDefects);
        }
    }

    bool CheckForOvality(double threshold)
    {
        cv::Mat& image = imageForOvality;
        std::vector<cv::Point> contour = GetCapContour(image); // Uses internal decolorization
        if (contour.empty()) return false; // Treat no contour as failure/defect or handle separately

        cv::drawContours(image, std::vector<std::vector<cv::Point>>{contour}, -1, cv::Scalar(255, 0, 0), 2);

        cv::RotatedRect ellipse = cv::fitEllipse(contour);
        float majorAxis = std::max(ellipse.size.width, ellipse.size.height);
        float minorAxis = std::min(ellipse.size.width, ellipse.size.height);
        float ratio = (majorAxis > 0) ? (minorAxis / majorAxis) : 0.0f;

        bool isOval = ratio < threshold;

        cv::Scalar color = isOval ? cv::Scalar(0, 0, 255) : cv::Scalar(0, 255, 0);
        cv::ellipse(image, ellipse, color, 2);

        std::string txt = "Ratio: " + std::to_string(ratio);
        cv::putText(image, txt, cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 1, color, 2);

        return isOval;
    }

    bool CheckForInclusions(
        double minArea,
        double maxArea,
        double circularityThresh
    )
    {
        cv::Mat& image = imageForInclusions;
        std::vector<cv::Point> contour = GetCapContour(image);
        if (contour.empty()) return false;

        cv::Mat gray;
        cv::cvtColor(image, gray, cv::ColorConversionCodes::COLOR_BGR2GRAY);
        cv::GaussianBlur(gray, gray, cv::Size(5, 5), 0);

        cv::RotatedRect ellipse = cv::fitEllipse(contour);
        cv::Point2f center = ellipse.center;
        // Logic from C#: 0.7 * (width + height) / 4.0 -> approx 70% of average radius
        float radius = 0.7f * (ellipse.size.width + ellipse.size.height) / 4.0f;

        cv::circle(image, center, static_cast<int>(radius), cv::Scalar(0, 255, 0), 2);

        cv::Mat mask = cv::Mat::zeros(gray.size(), CV_8UC1);
        cv::circle(mask, center, static_cast<int>(radius), cv::Scalar(255), -1);

        cv::Mat croppedRegion;
        gray.copyTo(croppedRegion, mask);

        cv::Mat binary, filteredBinary;
        // C#: AdaptiveThresholdTypes.MeanC, ThresholdTypes.BinaryInv, 11, 2
        cv::adaptiveThreshold(croppedRegion, binary, 255, cv::ADAPTIVE_THRESH_MEAN_C, cv::THRESH_BINARY_INV, 11, 2);

        // Remove noise
        cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(3, 3));
        cv::morphologyEx(binary, filteredBinary, cv::MORPH_OPEN, kernel, cv::Point(-1, -1), 1);

        std::vector<std::vector<cv::Point>> inclusionContours;
        cv::findContours(filteredBinary, inclusionContours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);

        bool inclusionsFound = false;
        for (const auto& cnt : inclusionContours) {
            double area = cv::contourArea(cnt);

            if (area > minArea && area < maxArea && IsCircularContour(cnt, circularityThresh)) {
                cv::Rect bbox = cv::boundingRect(cnt);
                cv::rectangle(image, bbox, cv::Scalar(0, 0, 255), 2);
                inclusionsFound = true;
            }
        }

        return inclusionsFound;
    }

    bool CheckForPaintDefects(double minArea, double whiteThreshold)
    {
        cv::Mat& image = imageForPaintDefects;
        cv::Mat hsv;
        cv::cvtColor(image, hsv, cv::COLOR_BGR2HSV);

        std::vector<cv::Point> capContour = GetCapContour(image);
        if (capContour.empty()) return false;

        cv::polylines(image, std::vector<std::vector<cv::Point>>{capContour}, true, cv::Scalar(255, 0, 0), 2);

        // Create Mask for the Cap
        cv::Mat capMask = cv::Mat::zeros(image.size(), CV_8UC1);
        cv::fillPoly(capMask, std::vector<std::vector<cv::Point>>{capContour}, cv::Scalar(255));

        // Logic from C#: Mask HSV
        cv::Mat maskedHSV;
        cv::bitwise_and(hsv, hsv, maskedHSV, capMask);

        // C#: InRange(0, 255*0.05...) -> Defines "Color". 
        // Note: OpenCV C++ HSV ranges are H:0-180, S:0-255, V:0-255
        cv::Scalar lower(0, 255 * 0.05, 255 * 0.05);
        cv::Scalar upper(180, 255 * 0.95, 255 * 0.95);

        cv::Mat whiteMask;
        cv::inRange(hsv, lower, upper, whiteMask); // This identifies "Colored" parts

        cv::Mat defectsMask;
        cv::bitwise_not(whiteMask, defectsMask); // This identifies "Not Colored" (White/Grey/Black) parts

        cv::Mat maskedDefects;
        cv::bitwise_and(defectsMask, capMask, maskedDefects); // Restrict to cap area

        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(maskedDefects, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

        bool defectsFound = false;
        for (const auto& cnt : contours) {
            if (cv::contourArea(cnt) > minArea) {
                // Check color intensity inside defect to confirm it is "white-ish" (paint missing or wrong)
                cv::Mat contourMask = cv::Mat::zeros(image.size(), CV_8UC1);
                cv::fillPoly(contourMask, std::vector<std::vector<cv::Point>>{cnt}, cv::Scalar(255));

                cv::Scalar meanColor = cv::mean(image, contourMask);

                // C# Logic: Math.Abs(meanColor.Val2 - 255) < minInpaintWhiteTgreshold
                // Val2 in OpenCVSharp (BGR) is Red channel.
                if (std::abs(meanColor[2] - 255.0) < whiteThreshold) {

                    std::vector<std::vector<cv::Point>> defectVec = { cnt };
                    cv::drawContours(image, defectVec, -1, cv::Scalar(0, 0, 255), 2);
                    cv::Rect bbox = cv::boundingRect(cnt);
                    cv::rectangle(image, bbox, cv::Scalar(0, 255, 255), 2);

                    defectsFound = true;
                }
            }
        }
        return defectsFound;
    }

    cv::Mat& getOvalityDebugImage() {
        return imageForOvality;
    }

    cv::Mat& getInclusionsDebugImage() {
        return imageForInclusions;
    }

    cv::Mat& getPaintDefectsDebugImage() {
        return imageForPaintDefects;
    }
private:
    void InitializeMorphologicalElements() {
        int morphSize = 4;
        int morphSize2 = 4;

        element1 = cv::getStructuringElement(
            cv::MORPH_RECT,
            cv::Size(2 * morphSize + 1, 2 * morphSize + 1),
            cv::Point(morphSize, morphSize)
        );

        element2 = cv::getStructuringElement(
            cv::MORPH_CROSS,
            cv::Size(2 * morphSize2 + 1, 2 * morphSize2 + 1),
            cv::Point(morphSize2, morphSize2)
        );
    }

    bool IsCircularContour(const std::vector<cv::Point>& contour, double threshold) {
        double perimeter = cv::arcLength(contour, true);
        double area = cv::contourArea(contour);
        if (perimeter == 0) return false;

        double circularity = (4 * CV_PI * area) / (perimeter * perimeter);
        return circularity > threshold;
    }

    std::vector<cv::Point> GetCapContour(const cv::Mat& image) {
        if (image.empty())
            return std::vector<cv::Point>();

        cv::Mat processed = image.clone();
        NonlinearBackgroundDecolorization(processed, BLUE_CAPS);

        std::vector<cv::Mat> channels;
        cv::split(processed, channels);

        cv::GaussianBlur(channels[0], channels[1], cv::Size(window, window), 4);
        cv::threshold(channels[1], channels[0], 128, 255, cv::THRESH_OTSU | cv::THRESH_BINARY);
        cv::morphologyEx(channels[0], channels[1], cv::MORPH_DILATE, element1);
        cv::morphologyEx(channels[1], channels[2], cv::MORPH_ERODE, element2);

        std::vector<std::vector<cv::Point>> contours;
        std::vector<cv::Vec4i> hierarchy;
        cv::findContours(
            channels[2],
            contours,
            hierarchy,
            cv::RETR_EXTERNAL,
            cv::CHAIN_APPROX_NONE
        );

        size_t maxInd = 0;
        size_t maxLength = 0;
        for (size_t i = 0; i < contours.size(); i++) {
            if (contours[i].size() > maxLength) {
                maxLength = contours[i].size();
                maxInd = i;
            }
        }

        return !contours.empty() ? contours[maxInd] : std::vector<cv::Point>();
    }

    void NonlinearBackgroundDecolorization(cv::Mat& img, uint8_t nWhite) const {
        if (img.empty() || img.type() != CV_8UC3)
            throw std::invalid_argument("Ожидается 3-канальное 8-битное изображение.");

        int total = img.rows * img.cols * 3;
        uint8_t* data = img.data;

        if (!isColored) {
            for (int i = 0; i < total; i++) {
                int val = (255 * data[i]) / nWhite;
                if (val > 255) val = 255;
                data[i] = static_cast<uint8_t>(val);
            }
        }
        else {
            for (int i = 0; i < total; i += 3) {
                int b = (255 * data[i]) / nWhite;
                int g = (255 * data[i + 1]) / nWhite;
                int r = (255 * data[i + 2]) / nWhite;

                if (b > 255) b = 255;
                if (g > 255) g = 255;
                if (r > 255) r = 255;

                data[i] = static_cast<uint8_t>(std::abs(b - ((g + r) >> 1)));
                data[i + 1] = static_cast<uint8_t>(g);
                data[i + 2] = static_cast<uint8_t>(r);
            }
        }
    }
};

class OgnCheckDefects
{
public:
    static bool compute(OgnCheckDefectsDatabase& db)
    {
        auto iActionGraph = omni::graph::action::getInterface();

        if (iActionGraph->getExecutionEnabled(inputs::tick.token(), db.getInstanceIndex())
            &&
            !iActionGraph->getExecutionEnabled(inputs::execIn.token(), db.getInstanceIndex()))
        {
            iActionGraph->setExecutionEnabled(outputs::tick.token(), db.getInstanceIndex());
            return true;
        }

        auto& imageData = db.inputs.imageData();
        if (imageData.size() == 0) {
            db.logWarning("Given an empty image for defects node. Skip execution");
            return false;
        }

        int32_t height = static_cast<int32_t>(db.inputs.imageHeight());
        int32_t width = static_cast<int32_t>(db.inputs.imageWidth());
        cv::Mat rgba8CapImage(
            height,
            width,
            CV_8UC4,
            // Appreciate because we won't change this array further.
            const_cast<uint8_t*>(imageData.data())
        );

       cv::Mat inputFrame;
       cv::cvtColor(rgba8CapImage, inputFrame, cv::COLOR_RGBA2BGR);

        CapProcessor processor = CapProcessor(
            db.inputs.enableOvalityCheck(),
            db.inputs.enableInclusionCheck(),
            db.inputs.enablePaintCheck(),
            inputFrame
        );

        bool isAnyDefectFound = false;

        if (db.inputs.enableOvalityCheck())
        {
            bool foundOvality = processor.CheckForOvality(db.inputs.ovalityThreshold());
            isAnyDefectFound = isAnyDefectFound || foundOvality;
            if (foundOvality)
            {
                db.logWarning("Detected an ovality defect");
            }

            cv::Mat& ovalityImage = processor.getOvalityDebugImage();
            cv::cvtColor(ovalityImage, ovalityImage, cv::COLOR_BGR2RGBA);
            db.outputs.ovalityImage().resize(ovalityImage.total() * ovalityImage.elemSize());
            std::memcpy(
                db.outputs.ovalityImage().data(),
                ovalityImage.data,
                ovalityImage.total() * ovalityImage.elemSize()
            );
        }
        if (db.inputs.enableInclusionCheck())
        {
            bool foundInclusions = processor.CheckForInclusions(
                db.inputs.minAreaInclusion(),
                db.inputs.maxAreaInclusion(),
                db.inputs.inclusionCircularity()
            );
            isAnyDefectFound = isAnyDefectFound || foundInclusions;
            if (foundInclusions)
            {
                db.logWarning("Detected an inclusions defect");
            }

            cv::Mat& inclusionsImage = processor.getInclusionsDebugImage();
            cv::cvtColor(inclusionsImage, inclusionsImage, cv::COLOR_BGR2RGBA);
            db.outputs.inclusionsImage().resize(inclusionsImage.total() * inclusionsImage.elemSize());
            std::memcpy(
                db.outputs.inclusionsImage().data(),
                inclusionsImage.data,
                inclusionsImage.total() * inclusionsImage.elemSize()
            );
        }
        if (db.inputs.enablePaintCheck())
        {
            bool foundPaintDefect = processor.CheckForPaintDefects(
                db.inputs.minAreaPaintDefect(),
                db.inputs.paintWhiteThreshold()
            );
            isAnyDefectFound = isAnyDefectFound || foundPaintDefect;
            if (foundPaintDefect) {
                db.logWarning("Detected a paint defect");
           }

            cv::Mat& paintDefectsImage = processor.getPaintDefectsDebugImage();
            cv::cvtColor(paintDefectsImage, paintDefectsImage, cv::COLOR_BGR2RGBA);
            db.outputs.paintImage().resize(paintDefectsImage.total() * paintDefectsImage.elemSize());
            std::memcpy(
                db.outputs.paintImage().data(),
                paintDefectsImage.data,
                paintDefectsImage.total() * paintDefectsImage.elemSize()
            );
            
        }

        if (isAnyDefectFound)
        {
            iActionGraph->setExecutionEnabled(outputs::defectDetected.token(), kAccordingToContextIndex);
        }
        else
        {
            iActionGraph->setExecutionEnabled(outputs::execOut.token(), kAccordingToContextIndex);
        }

        db.outputs.imageWidth() = db.inputs.imageWidth();
        db.outputs.imageHeight() = db.inputs.imageHeight();
        db.outputs.imageFormat() = static_cast<uint32_t>(carb::Format::eRGBA8_UNORM);

        return true;
    }
};

REGISTER_OGN_NODE()
} // nodes
} // simulation
} // kvantron
