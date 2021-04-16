/*********************************************************************
* Software License Agreement (BSD License)
*
*  Copyright (c) 20012, Willow Garage, Inc.
*  All rights reserved.
*
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions
*  are met:
*
*   * Redistributions of source code must retain the above copyright
*     notice, this list of conditions and the following disclaimer.
*   * Redistributions in binary form must reproduce the above
*     copyright notice, this list of conditions and the following
*     disclaimer in the documentation and/or other materials provided
*     with the distribution.
*   * Neither the name of the Willow Garage nor the names of its
*     contributors may be used to endorse or promote products derived
*     from this software without specific prior written permission.
*
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
*  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
*  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
*  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
*  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
*  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
*  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
*  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
*  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
*  POSSIBILITY OF SUCH DAMAGE.
*********************************************************************/

#include <compressed_image_transport/compression_common.h>
#include <stdexcept>
#include <cv_bridge/cv_bridge.h>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <ros/ros.h>
#include <turbojpeg.h>

namespace enc = sensor_msgs::image_encodings;

namespace compressed_image_transport {

    //by xqms https://github.com/xqms/image_transport_plugins/tree/turbojpeg
    sensor_msgs::ImagePtr decompressJPEG(const std::vector<uint8_t>& data, const std::string& source_encoding, const std_msgs::Header& header)
    {
      tjhandle tj_ = tjInitDecompress();

      int width, height, jpegSub, jpegColor;

      // Old TurboJPEG require a const_cast here. This was fixed in TurboJPEG 1.5.
      uint8_t* src = const_cast<uint8_t*>(data.data());

      if (tjDecompressHeader3(tj_, src, data.size(), &width, &height, &jpegSub, &jpegColor) != 0)
        return sensor_msgs::ImagePtr(); // If we cannot decode the JPEG header, silently fall back to OpenCV

      sensor_msgs::ImagePtr ret(new sensor_msgs::Image);
      ret->header = header;
      ret->width = width;
      ret->height = height;
      ret->encoding = source_encoding;

      int pixelFormat;

      if (source_encoding == enc::MONO8)
      {
        ret->data.resize(height*width);
        ret->step = ret->width;
        pixelFormat = TJPF_GRAY;
      }
      else if (source_encoding == enc::RGB8)
      {
        ret->data.resize(height*width*3);
        ret->step = width*3;
        pixelFormat = TJPF_RGB;
      }
      else if (source_encoding == enc::BGR8)
      {
        ret->data.resize(height*width*3);
        ret->step = width*3;
        pixelFormat = TJPF_BGR;
      }
      else if (source_encoding == enc::RGBA8)
      {
        ret->data.resize(height*width*4);
        ret->step = width*4;
        pixelFormat = TJPF_RGBA;
      }
      else if (source_encoding == enc::BGRA8)
      {
        ret->data.resize(height*width*4);
        ret->step = width*4;
        pixelFormat = TJPF_BGRA;
      }
      else if (source_encoding.empty())
      {
        // Autodetect based on image
        if(jpegColor == TJCS_GRAY)
        {
          ret->data.resize(height*width);
          ret->step = width;
          ret->encoding = enc::MONO8;
          pixelFormat = TJPF_GRAY;
        }
        else
        {
          ret->data.resize(height*width*3);
          ret->step = width*3;
          ret->encoding = enc::RGB8;
          pixelFormat = TJPF_RGB;
        }
      }
      else
      {
        ROS_WARN_THROTTLE(10.0, "Encountered a source encoding that is not supported by TurboJPEG: '%s'", source_encoding.c_str());
        tjDestroy(tj_);
        return sensor_msgs::ImagePtr();
      }

      if (tjDecompress2(tj_, src, data.size(), ret->data.data(), width, 0, height, pixelFormat, 0) != 0)
      {
        ROS_WARN_THROTTLE(10.0, "Could not decompress data using TurboJPEG, falling back to OpenCV");
        tjDestroy(tj_);
        return sensor_msgs::ImagePtr();
      }
      tjDestroy(tj_);
      return ret;
    }

    sensor_msgs::ImagePtr decodeCompressedImage(const sensor_msgs::CompressedImageConstPtr &image, int decode_flag) {
        if (!image)
            throw std::runtime_error("Call to decode a compressed image received a NULL pointer.");

        std::string image_encoding;
        std::string compressed_encoding;
        {
          const size_t split_pos = image->format.find(';');
          if (split_pos != std::string::npos)
          {
            image_encoding = image->format.substr(0, split_pos);
            compressed_encoding = image->format.substr(split_pos);
          }
        }

        // Try TurboJPEG first (if the first bytes look like JPEG)
        if(image->data.size() > 4 && image->data[0] == 0xFF && image->data[1] == 0xD8)
        {
          sensor_msgs::ImagePtr decoded = decompressJPEG(image->data, image_encoding, image->header);
          if(decoded)
          {
            return decoded;
          }
        }

        cv_bridge::CvImagePtr cv_ptr(new cv_bridge::CvImage);

        // Copy message header
        cv_ptr->header = image->header;

        // Decode color/mono image
        cv_ptr->image = cv::imdecode(cv::Mat(image->data), decode_flag);

        // Assign image encoding string
        const size_t split_pos = image->format.find(';');
        if (split_pos == std::string::npos) {
            // Older version of compressed_image_transport does not signal image format
            switch (cv_ptr->image.channels()) {
                case 1:
                    cv_ptr->encoding = enc::MONO8;
                    break;
                case 3:
                    cv_ptr->encoding = enc::BGR8;
                    break;
                default: {
                    std::stringstream ss;
                    ss << "Unsupported number of channels: " << cv_ptr->image.channels();
                    throw std::runtime_error(ss.str());
                }
            }
        } else {
            cv_ptr->encoding = image_encoding;

            if (enc::isColor(image_encoding)) {
                bool compressed_bgr_image = (compressed_encoding.find("compressed bgr") != std::string::npos);

                // Revert color transformation
                if (compressed_bgr_image) {
                    // if necessary convert colors from bgr to rgb
                    if ((image_encoding == enc::RGB8) || (image_encoding == enc::RGB16))
                        cv::cvtColor(cv_ptr->image, cv_ptr->image, CV_BGR2RGB);

                    if ((image_encoding == enc::RGBA8) || (image_encoding == enc::RGBA16))
                        cv::cvtColor(cv_ptr->image, cv_ptr->image, CV_BGR2RGBA);

                    if ((image_encoding == enc::BGRA8) || (image_encoding == enc::BGRA16))
                        cv::cvtColor(cv_ptr->image, cv_ptr->image, CV_BGR2BGRA);
                } else {
                    // if necessary convert colors from rgb to bgr
                    if ((image_encoding == enc::BGR8) || (image_encoding == enc::BGR16))
                        cv::cvtColor(cv_ptr->image, cv_ptr->image, CV_RGB2BGR);

                    if ((image_encoding == enc::BGRA8) || (image_encoding == enc::BGRA16))
                        cv::cvtColor(cv_ptr->image, cv_ptr->image, CV_RGB2BGRA);

                    if ((image_encoding == enc::RGBA8) || (image_encoding == enc::RGBA16))
                        cv::cvtColor(cv_ptr->image, cv_ptr->image, CV_RGB2RGBA);
                }
            }
        }

        if ((cv_ptr->image.rows > 0) && (cv_ptr->image.cols > 0))
            return cv_ptr->toImageMsg();
        else {
            std::stringstream ss;
            ss << "Could not extract meaningful image. One of the dimensions was 0. Rows: "
               << cv_ptr->image.rows << ", columns: " << cv_ptr->image.cols << ".";
            throw std::runtime_error(ss.str());
        }
    }

    sensor_msgs::CompressedImagePtr compressJPEG(const sensor_msgs::Image &image, std::vector<int> params){
        int width, step, height, pixelFormat, jpegSubsamp;
        boost::shared_ptr <sensor_msgs::CompressedImage> compressed(new sensor_msgs::CompressedImage);
        compressed->header = image.header;
        compressed->format = image.encoding;
        compressed->format += "; jpeg compressed " + image.encoding;

        width = image.width;
        height = image.height;
        step = image.step;
        jpegSubsamp = TJSAMP_444;

        tjhandle tj_ = tjInitCompress();
        uint8_t* src = const_cast<uint8_t*>(image.data.data());
        unsigned char* jpegBuf = NULL;
        long unsigned int jpegSize = 0;
        int jpeg_quality = 95;
        for(std::size_t i = 0; i < params.size()-1; i += 2) {
            if(params[i] ==  cv::IMWRITE_JPEG_QUALITY)
              jpeg_quality = params[i+1];
        }

        if (image.encoding == enc::MONO8)
        {
          pixelFormat = TJPF_GRAY;
          jpegSubsamp = TJSAMP_GRAY;
        }
        else if (image.encoding == enc::RGB8)
        {
          pixelFormat = TJPF_RGB;
        }
        else if (image.encoding == enc::BGR8)
        {
          pixelFormat = TJPF_BGR;
        }
        else if (image.encoding == enc::RGBA8)
        {
          pixelFormat = TJPF_RGBA;
        }
        else if (image.encoding == enc::BGRA8)
        {
          pixelFormat = TJPF_BGRA;
        }
        else
        {
          ROS_WARN_THROTTLE(10.0, "Encountered a source encoding that is not supported by TurboJPEG: '%s'", image.encoding.c_str());
          tjDestroy(tj_);
          delete jpegBuf;
          return sensor_msgs::CompressedImagePtr();
        }

        if(0 == tjCompress2(tj_, src,width, step, height, pixelFormat, &jpegBuf, &jpegSize, jpegSubsamp, jpeg_quality, TJFLAG_FASTDCT)){
            tjDestroy(tj_);
            ROS_DEBUG("Compressed Image Transport - Codec: jpg; via TurboJPEG");
            compressed->data = std::vector<unsigned char>(jpegBuf, jpegBuf + jpegSize);
            delete jpegBuf;
            return  compressed;
        }
        tjDestroy(tj_);
        delete jpegBuf;
        ROS_DEBUG("Compressed Image Transport - Codec: jpg; via TurboJPEG failed. Falling back to opencv");
        return sensor_msgs::CompressedImagePtr();
    }

    sensor_msgs::CompressedImagePtr encodeImage(const sensor_msgs::Image &message, compressionFormat encode_flag, std::vector<int> params) {

        boost::shared_ptr <sensor_msgs::CompressedImage> compressed(new sensor_msgs::CompressedImage);
        compressed->header = message.header;
        compressed->format = message.encoding;
        // Bit depth of image encoding
        int bitDepth = enc::bitDepth(message.encoding);
        int numChannels = enc::numChannels(message.encoding);

        switch (encode_flag) {
            case JPEG:
            {
                compressed->format += "; jpeg compressed ";
                // Check input format
                if ((bitDepth == 8) || (bitDepth == 16)) {

                    // No conversion of color images to bgr8
                    // sensor_msgs::CompressedImagePtr encoded = compressJPEG(message, params);
                    // if (encoded){
                    //     return encoded;
                    // }

                    // Target image format
                    std::string targetFormat;
                    if (enc::isColor(message.encoding)) {
                        // convert color images to BGR8 format
                        targetFormat = "bgr8";
                        compressed->format += targetFormat;
                    }
                    // OpenCV-ros bridge
                    boost::shared_ptr<void> tracked_object;
                    cv_bridge::CvImageConstPtr cv_ptr = cv_bridge::toCvShare(message, tracked_object, targetFormat);

                    // Compress image
                    if (cv::imencode(".jpg", cv_ptr->image, compressed->data, params)) {

                        float cRatio = (float) (cv_ptr->image.rows * cv_ptr->image.cols * cv_ptr->image.elemSize())
                                       / (float) compressed->data.size();
                        ROS_DEBUG("Compressed Image Transport - Codec: jpg, Compression Ratio: 1:%.2f (%lu bytes)", cRatio,
                                  compressed->data.size());
                    } else {
                        throw std::runtime_error("cv::imencode (jpeg) failed on input image");
                    }
                    return compressed;
                } else {
                  throw std::runtime_error("Compressed Image Transport - JPEG compression requires 8/16-bit color format (input format is: "+ message.encoding +
 ")");
                }
            }
            case PNG:
            {
                // Update ros message format header
                compressed->format += "; png compressed ";
                // Check input format
                if ((bitDepth == 8) || (bitDepth == 16)) {

                  // Target image format
                  std::ostringstream targetFormat;
                  if (enc::isColor(message.encoding)) {
                    // convert color images to RGB domain
                    targetFormat << "bgr" << bitDepth;
                    compressed->format += targetFormat.str();
                  }

                  boost::shared_ptr<void> tracked_object;
                  cv_bridge::CvImageConstPtr cv_ptr = cv_bridge::toCvShare(message, tracked_object, targetFormat.str());

                  // Compress image
                  if (cv::imencode(".png", cv_ptr->image, compressed->data, params)) {

                    float cRatio = (float)(cv_ptr->image.rows * cv_ptr->image.cols * cv_ptr->image.elemSize())/ (float)compressed->data.size();
                    ROS_DEBUG("Compressed Image Transport - Codec: png, Compression Ratio: 1:%.2f (%lu bytes)", cRatio, compressed->data.size());
                  } else {
                    throw std::runtime_error("cv::imencode (png) failed on input image");
                  }
                  // Publish message
                  return compressed;
                } else {
                  throw std::runtime_error("Compressed Image Transport - PNG compression requires 8/16-bit encoded color format (input format is: "+  message.encoding +")");
                  break;
                }
            }
            default:
            {
              throw std::runtime_error("Unknown compression type, valid options are 'jpeg(0)' and 'png(1)'");
              break;
            }
        }
        return compressed;
    }

}
