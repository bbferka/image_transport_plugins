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

#ifndef COMPRESSED_IMAGE_TRANSPORT_COMPRESSION_COMMON
#define COMPRESSED_IMAGE_TRANSPORT_COMPRESSION_COMMON

#include <sensor_msgs/CompressedImage.h>
#include <sensor_msgs/Image.h>

namespace compressed_image_transport
{

// Compression formats
enum compressionFormat
{
  UNDEFINED = -1, JPEG, PNG
};

sensor_msgs::ImagePtr decompressJPEG(const std::vector<uint8_t>& data, const std::string&source_encoding, const std_msgs::Header& header);


// standadlone decoding function
sensor_msgs::ImagePtr decodeCompressedImage(const sensor_msgs::CompressedImageConstPtr& image, int decode_flag);


/**
 * @brief encodeImage standadlone encoding function wrapping around cv::imencode for compressin sensor_msgs::Image messages
 * @param iamge the image message to encode
 * @param encode_flag one of compressionFormat::JPEG or compressionFormat::PNG
 * @param params Format-specific parameters. See cv::imwrite and cv::ImwriteFlags.
 * @return
 */
sensor_msgs::CompressedImagePtr encodeImage(const sensor_msgs::Image &image, compressionFormat encode_flag, std::vector<int> params = std::vector<int>());

} //namespace compressed_image_transport

#endif
