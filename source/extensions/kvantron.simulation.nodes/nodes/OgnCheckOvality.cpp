#include "OgnCheckOvalityDatabase.h"
#include <algorithm>
#include <carb/RenderingTypes.h>
#include <cmath>
#include <cstdint>
#include <omni/graph/action/IActionGraph.h>
#include <omni/graph/core/iComputeGraph.h>
#include <opencv2/highgui.hpp>
#include <opencv2/core.hpp>
#include <opencv2/core/hal/interface.h>
#include <opencv2/core/mat.hpp>
#include <opencv2/core/matx.hpp>
#include <opencv2/core/operations.hpp>
#include <opencv2/core/types.hpp>
#include <opencv2/imgproc.hpp>
#include <stdexcept>
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

public:
    static constexpr uint8_t BLUE_CAPS = 80;
    static constexpr uint8_t YELLOW_CAPS = 160;
    static constexpr uint8_t GOLD_CAPS = 160;
    static constexpr uint8_t WHITE_CAPS = 160;
    static constexpr int window = 15;

    CapProcessor() {
        InitializeMorphologicalElements();
    }

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

    bool IsCircular(const cv::Mat& image, double ovalityThreshold)
    {
        std::vector<cv::Point> largestContourOvality = GetCapContour(image);
        std::vector<std::vector<cv::Point>> temp = { largestContourOvality };

        cv::drawContours(
            image,
            temp,
            -1,
            cv::Scalar(255, 0, 0),
            2
        );

        cv::RotatedRect contourOuterRect = cv::fitEllipse(largestContourOvality);
        float majorAxis = std::max(contourOuterRect.size.width, contourOuterRect.size.height);
        float minorAxis = std::min(contourOuterRect.size.width, contourOuterRect.size.height);
        float circularity = minorAxis / majorAxis;
        bool isCircular = circularity >= ovalityThreshold;

        cv::Scalar color = isCircular
            ? cv::Scalar(0, 255, 0)
            : cv::Scalar(0, 0, 255);

        cv::ellipse(image, contourOuterRect, color, 2);
        cv::putText(
            image,
            cv::format("Circularity: %.5f", circularity),
            cv::Point(10, 30),
            cv::HersheyFonts::FONT_HERSHEY_SIMPLEX,
            1,
            color,
            2
        );
        
        return isCircular;
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

class OgnCheckOvality
{
public:
    static bool compute(OgnCheckOvalityDatabase& db)
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
            db.logError("Given an empty image for CheckOvality node. Skip execution");
            return false;
        }

        int32_t height = static_cast<int32_t>(db.inputs.imageHeight());
        int32_t width = static_cast<int32_t>(db.inputs.imageWidth());
        double ovalityThreshold = db.inputs.ovalityThreshold();

        cv::Mat rgba8CapImage(
            height,
            width,
            CV_8UC4,
            // Appreciate because we won't change this array further.
            const_cast<uint8_t*>(imageData.data())
        );

       cv::Mat bgr8CapImage;
       cv::cvtColor(rgba8CapImage, bgr8CapImage, cv::COLOR_RGBA2BGR);

        CapProcessor processor = CapProcessor();
        bool isCapCircular = processor.IsCircular(bgr8CapImage, ovalityThreshold);

        db.outputs.imageWidth() = db.inputs.imageWidth();
        db.outputs.imageHeight() = db.inputs.imageHeight();
        db.outputs.imageFormat() = static_cast<uint32_t>(carb::Format::eRGBA8_UNORM);

        cv::Mat rgbaOut(
            height,
            width,
            CV_8UC4
        );
        cv::cvtColor(bgr8CapImage, rgbaOut, cv::COLOR_BGR2RGBA);
        rgbaOut.forEach<cv::Vec4b>([](cv::Vec4b& p, const int*) { p[3] = 255; });
        db.outputs.processedImage().resize(rgbaOut.total() * rgbaOut.elemSize());
        std::memcpy(db.outputs.processedImage().data(), rgbaOut.data, rgbaOut.total() * rgbaOut.elemSize());


        //db.outputs.processedImage().resize(rgba8CapImage.total() * rgba8CapImage.elemSize());

        //cv::Mat temp;
        //cv::cvtColor(bgr8CapImage, temp, cv::COLOR_BGR2RGBA);

        //std::copy(
        //    temp.begin<uint8_t>(),
        //    temp.end<uint8_t>(),
        //    db.outputs.processedImage().begin()
        //);

        //cv::Mat temp2(
        //    height,
        //    width,
        //    CV_8UC4,
        //    // Appreciate because we won't change this array further.
        //    const_cast<uint8_t*>(db.outputs.processedImage().data())
        //);

        //cv::imshow("temp2", temp2);
        //cv::waitKey();
        //cv::destroyAllWindows();

        if (!isCapCircular) {
            
            iActionGraph->setExecutionEnabled(outputs::defectDetected.token(), kAccordingToContextIndex);
        }
        else
        {
            iActionGraph->setExecutionEnabled(outputs::execOut.token(), kAccordingToContextIndex);
        }


        return true;
    }
};

REGISTER_OGN_NODE()
} // nodes
} // simulation
} // kvantron
