#include "service/reconstruction_service.hpp"

#include "logger/logger_macros.hpp"
#include "runtime/handler_context.hpp"
#include "window/window.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <opencv2/calib3d.hpp>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <vector>

namespace fs = std::filesystem;

namespace service::reconstruction
{

namespace
{

uint64_t grayToBinary(uint64_t num)
{
    uint64_t mask = num >> 1;
    while (mask != 0)
    {
        num = num ^ mask;
        mask = mask >> 1;
    }
    return num;
}

} // namespace

void run(runtime::ReconstructionHandlerContext& ctx, win::Window& target_window,
         const cmd::CmdReconstruct& command)
{
    if (target_window.id() != ctx.preview_window_id)
    {
        return;
    }

    LOG_INFO("Reconstruct: 3D復元を開始します (非平行対応/詳細ログモード)...");

    cv::Mat K1, D1, K2, D2, R, T;
    try
    {
        cv::FileStorage fs_in(command.calib_file, cv::FileStorage::READ);
        if (!fs_in.isOpened())
        {
            LOG_ERROR("File Open Error: {}", command.calib_file);
            return;
        }
        fs_in["K1"] >> K1;
        fs_in["D1"] >> D1;
        fs_in["K2"] >> K2;
        fs_in["D2"] >> D2;
        fs_in["R"] >> R;
        fs_in["T"] >> T;
    }
    catch (...)
    {
        return;
    }

    cv::Mat P1 = cv::Mat::zeros(3, 4, CV_64F);
    K1.copyTo(P1(cv::Rect(0, 0, 3, 3)));

    cv::Mat R_mat, T_mat;
    R.copyTo(R_mat);
    T.copyTo(T_mat);
    cv::Mat RT;
    cv::hconcat(R_mat, T_mat, RT);
    cv::Mat P2 = K2 * RT;

    cv::Mat tx =
        (cv::Mat_<double>(3, 3) << 0, -T.at<double>(2), T.at<double>(1),
         T.at<double>(2), 0, -T.at<double>(0), -T.at<double>(1), T.at<double>(0), 0);
    cv::Mat E = tx * R;
    cv::Mat F = K2.inv().t() * E * K1.inv();

    auto get_files = [](const std::string& dir)
    {
        std::vector<std::string> files;
        if (fs::exists(dir))
        {
            for (auto& e : fs::directory_iterator(dir))
            {
                if (e.is_regular_file())
                {
                    files.push_back(e.path().string());
                }
            }
        }
        std::sort(files.begin(), files.end());
        return files;
    };

    auto fL = get_files(command.scan_dir_L);
    auto fR = get_files(command.scan_dir_R);
    if (fL.empty())
    {
        LOG_ERROR("画像が見つかりません");
        return;
    }

    cv::Size sz = cv::imread(fL[0], 0).size();

    LOG_INFO("Reconstruct: デコード中 (Raw画像 / 枚数: {})...", fL.size());
    std::vector<uint64_t> codeL(sz.area(), 0), codeR(sz.area(), 0);
    cv::Mat maskL = cv::Mat::zeros(sz, CV_8U), maskR = cv::Mat::zeros(sz, CV_8U);
    cv::Mat minL(sz, CV_8U, cv::Scalar(255)), maxL(sz, CV_8U, cv::Scalar(0));
    cv::Mat minR(sz, CV_8U, cv::Scalar(255)), maxR(sz, CV_8U, cv::Scalar(0));

    for (size_t i = 0; i < fL.size(); ++i)
    {
        if (i >= 64)
            break;
        uint64_t bit = 1ULL << i;
        cv::Mat imgL = cv::imread(fL[i], 0);
        cv::Mat imgR = cv::imread(fR[i], 0);
        cv::min(minL, imgL, minL);
        cv::max(maxL, imgL, maxL);
        cv::min(minR, imgR, minR);
        cv::max(maxR, imgR, maxR);

        const uint8_t* pL = imgL.data;
        const uint8_t* pR = imgR.data;
        const uint8_t* pMinL = minL.data;
        const uint8_t* pMaxL = maxL.data;
        const uint8_t* pMinR = minR.data;
        const uint8_t* pMaxR = maxR.data;
        for (int p = 0; p < sz.area(); ++p)
        {
            if (pL[p] > (pMinL[p] + pMaxL[p]) / 2)
                codeL[p] |= bit;
            if (pR[p] > (pMinR[p] + pMaxR[p]) / 2)
                codeR[p] |= bit;
        }
    }

    int contrast_th = 1;
    cv::Mat visCodeL = cv::Mat::zeros(sz, CV_8UC3);
    cv::Mat visCodeR = cv::Mat::zeros(sz, CV_8UC3);
    std::map<uint64_t, std::vector<cv::Point2f>> right_code_map;

    for (int i = 0; i < sz.area(); ++i)
    {
        int y = i / sz.width;
        int x = i % sz.width;

        if (maxL.data[i] - minL.data[i] > contrast_th)
            maskL.data[i] = 255;
        if (maxR.data[i] - minR.data[i] > contrast_th)
            maskR.data[i] = 255;

        codeL[i] = grayToBinary(codeL[i]);
        codeR[i] = grayToBinary(codeR[i]);

        if (maskL.data[i])
        {
            visCodeL.at<cv::Vec3b>(y, x) = cv::Vec3b((codeL[i] * 13) % 180, 255, 255);
        }
        if (maskR.data[i])
        {
            visCodeR.at<cv::Vec3b>(y, x) = cv::Vec3b((codeR[i] * 13) % 180, 255, 255);
            right_code_map[codeR[i]].push_back(cv::Point2f(x, y));
        }
    }

    cv::cvtColor(visCodeL, visCodeL, cv::COLOR_HSV2BGR);
    cv::cvtColor(visCodeR, visCodeR, cv::COLOR_HSV2BGR);
    cv::imwrite("debug_code_L.png", visCodeL);
    cv::imwrite("debug_code_R.png", visCodeR);
    cv::imwrite("debug_mask_L.png", maskL);
    cv::imwrite("debug_mask_R.png", maskR);

    int validL = cv::countNonZero(maskL);
    int validR = cv::countNonZero(maskR);
    LOG_INFO("Log: 有効画素数 L: {} / R: {}", validL, validR);
    LOG_INFO("Log: 右画像のユニークなコード数: {}", right_code_map.size());

    LOG_INFO("Reconstruct: エピポーラマッチング (1bit誤差許容/広域探索)...");
    std::vector<cv::Vec3f> pts3d;

    cv::Mat debugMatchL = cv::imread(fL[0]);
    cv::Mat debugMatchR = cv::imread(fR[0]);
    int matched_count = 0;

    for (int y = 0; y < sz.height; y++)
    {
        for (int x = 0; x < sz.width; x++)
        {
            if (!maskL.at<uint8_t>(y, x))
                continue;

            uint64_t c_origin = codeL[y * sz.width + x];
            std::vector<uint64_t> search_codes;
            search_codes.reserve(48);
            search_codes.push_back(c_origin);
            for (int i = 0; i < 46; ++i)
                search_codes.push_back(c_origin ^ (1ULL << i));

            std::vector<cv::Point2f> src_pts = {cv::Point2f(x, y)};
            std::vector<cv::Point2f> dst_pts;
            cv::undistortPoints(src_pts, dst_pts, K1, D1, cv::noArray(), K1);
            cv::Point2f pL = dst_pts[0];

            double a = F.at<double>(0, 0) * pL.x + F.at<double>(0, 1) * pL.y +
                       F.at<double>(0, 2);
            double b = F.at<double>(1, 0) * pL.x + F.at<double>(1, 1) * pL.y +
                       F.at<double>(1, 2);
            double c_line = F.at<double>(2, 0) * pL.x + F.at<double>(2, 1) * pL.y +
                            F.at<double>(2, 2);
            double norm = std::sqrt(a * a + b * b);

            cv::Point2f best_pR_raw;
            double min_dist = 20.0;
            bool found = false;

            for (uint64_t c_target : search_codes)
            {
                auto it = right_code_map.find(c_target);
                if (it == right_code_map.end())
                    continue;

                for (const auto& pR_raw : it->second)
                {
                    double dist = std::abs(a * pR_raw.x + b * pR_raw.y + c_line) / norm;
                    if (dist < min_dist)
                    {
                        min_dist = dist;
                        best_pR_raw = pR_raw;
                        found = true;
                    }
                }
            }

            if (found)
            {
                matched_count++;
                if (matched_count % 100 == 0)
                {
                    cv::circle(debugMatchL, cv::Point(x, y), 1, cv::Scalar(0, 255, 0),
                               -1);
                    cv::circle(debugMatchR, best_pR_raw, 1, cv::Scalar(0, 255, 0), -1);
                }

                std::vector<cv::Point2f> src_R = {best_pR_raw};
                std::vector<cv::Point2f> dst_R;
                cv::undistortPoints(src_R, dst_R, K2, D2, cv::noArray(), K2);

                std::vector<cv::Point2f> pt1 = {pL};
                std::vector<cv::Point2f> pt2 = {dst_R[0]};
                cv::Mat pt4D;
                cv::triangulatePoints(P1, P2, pt1, pt2, pt4D);

                float w = pt4D.at<float>(3, 0);
                if (std::abs(w) > 1e-5)
                {
                    float X = pt4D.at<float>(0, 0) / w;
                    float Y = pt4D.at<float>(1, 0) / w;
                    float Z = pt4D.at<float>(2, 0) / w;

                    if (Z > 100 && Z < 600)
                    {
                        pts3d.push_back(cv::Vec3f(X, Y, Z));
                    }
                }
            }
        }
    }
    LOG_INFO("Reconstruct: マッチング候補数: {}, 3D化成功数: {}", matched_count,
             pts3d.size());

    cv::imwrite("debug_match_L.png", debugMatchL);
    cv::imwrite("debug_match_R.png", debugMatchR);

    {
        std::ofstream ply(command.output_ply);
        ply << "ply\nformat ascii 1.0\nelement vertex " << pts3d.size()
            << "\nproperty float x\nproperty float y\nproperty float z\nend_header\n";
        for (const auto& p : pts3d)
            ply << p[0] << " " << p[1] << " " << p[2] << "\n";
        LOG_INFO("Saved PLY: {}", command.output_ply);
    }

    {
        cv::Mat depth_map = cv::Mat::zeros(sz, CV_8UC1);
        cv::Mat color_map;
        float min_z = 10000.0f, max_z = 0.0f;
        for (const auto& p : pts3d)
        {
            if (p[2] < min_z)
                min_z = p[2];
            if (p[2] > max_z)
                max_z = p[2];
        }
        double fx = K1.at<double>(0, 0), fy = K1.at<double>(1, 1),
               cx = K1.at<double>(0, 2), cy = K1.at<double>(1, 2);
        for (const auto& p : pts3d)
        {
            if (p[2] <= 0)
                continue;
            int u = static_cast<int>(fx * p[0] / p[2] + cx);
            int v = static_cast<int>(fy * p[1] / p[2] + cy);
            if (u >= 0 && u < sz.width && v >= 0 && v < sz.height)
            {
                float norm = (p[2] - min_z) / (max_z - min_z + 1e-5f);
                depth_map.at<uint8_t>(v, u) = static_cast<uint8_t>((1.0f - norm) * 255);
            }
        }
        cv::applyColorMap(depth_map, color_map, cv::COLORMAP_JET);
        cv::Mat mask;
        cv::compare(depth_map, 0, mask, cv::CMP_EQ);
        color_map.setTo(cv::Scalar(0, 0, 0), mask);
        cv::imwrite("reconstruction_depth.png", depth_map);
        cv::imwrite("reconstruction_color.png", color_map);
        LOG_INFO("Saved Color Depth: reconstruction_color.png");
    }

    {
        cv::FileStorage fs_out("reconstruction_points.yml", cv::FileStorage::WRITE);
        if (fs_out.isOpened())
        {
            fs_out << "points" << cv::Mat(pts3d);
            fs_out.release();
            LOG_INFO("Saved YML: reconstruction_points.yml");
        }
    }
}

} // namespace service::reconstruction
