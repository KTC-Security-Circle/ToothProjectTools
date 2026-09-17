#include "calibration/camera_projector_calibration.hpp"
#include "calibration/camera_projector_calibration_service.hpp"
#include "calibration/projector_calibrator.hpp"
#include "scan/scan_dataset_validator.hpp"

#include <cassert>
#include <cstdlib>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <opencv2/calib3d.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <stdexcept>

namespace
{
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
#undef assert
#define assert(condition) require(static_cast<bool>(condition), #condition)
cv::Mat checkerboard(cv::Size corners)
{
    constexpr int square = 40;
    cv::Mat image((corners.height + 1) * square, (corners.width + 1) * square, CV_8UC1, cv::Scalar{255});
    for (int y=0; y<=corners.height; ++y) for (int x=0; x<=corners.width; ++x)
        if ((x+y)%2==0) cv::rectangle(image, {x*square,y*square,square,square}, cv::Scalar{0}, cv::FILLED);
    return image;
}

void testObservationAndMovement()
{
    const cv::Size board{10,7};
    const auto image=checkerboard(board);
    cv::Mat px(image.size(),CV_32S), py(image.size(),CV_32S), mask(image.size(),CV_8UC1,cv::Scalar{255});
    for(int y=0;y<image.rows;++y) for(int x=0;x<image.cols;++x) {
        px.at<int>(y,x)=x*479/(image.cols-1); py.at<int>(y,x)=y*269/(image.rows-1); }
    const auto valid=calib::projector::makeObservation(image,image,px,py,mask,board,12.5,0.1,0.1);
    assert(valid.valid && valid.observation.object_points.size()==70);
    const cv::Mat transform=(cv::Mat_<double>(2,3)<<1,0,5,0,1,0);
    cv::Mat moved; cv::warpAffine(image,moved,transform,image.size(),cv::INTER_NEAREST,
                                  cv::BORDER_CONSTANT,cv::Scalar{255});
    const auto rejected=calib::projector::makeObservation(image,moved,px,py,mask,board,12.5,1.0,2.0);
    assert(!rejected.valid && rejected.error=="board moved during pattern capture");
}

calib::projector::CalibrationResult syntheticSolve(std::vector<calib::projector::CalibrationObservation>& observations)
{
    const cv::Mat camera_k=(cv::Mat_<double>(3,3)<<700,0,320,0,700,240,0,0,1);
    const cv::Mat projector_k=(cv::Mat_<double>(3,3)<<550,0,240,0,540,135,0,0,1);
    const cv::Mat d=cv::Mat::zeros(1,5,CV_64F);
    const cv::Mat r_cp=(cv::Mat_<double>(3,3)<<0.9998,0,0.02,0,1,0,-0.02,0,0.9998);
    const cv::Mat t_cp=(cv::Mat_<double>(3,1)<<120,2,5);
    for(int pose=0;pose<6;++pose) {
        calib::projector::CalibrationObservation o;
        for(int y=0;y<7;++y) for(int x=0;x<10;++x) o.object_points.emplace_back(x*12.5f,y*12.5f,0);
        cv::Mat rv=(cv::Mat_<double>(3,1)<<0.03*pose,-0.02+0.01*pose,0.01*pose);
        cv::Mat tv=(cv::Mat_<double>(3,1)<<-50+15*pose,-35+8*pose,550+45*pose);
        cv::projectPoints(o.object_points,rv,tv,camera_k,d,o.camera_points);
        cv::Mat r_board; cv::Rodrigues(rv,r_board); cv::Mat r_projector=r_cp*r_board;
        cv::Mat t_projector=r_cp*tv+t_cp; cv::Mat rv_projector; cv::Rodrigues(r_projector,rv_projector);
        cv::projectPoints(o.object_points,rv_projector,t_projector,projector_k,d,o.projector_points);
        observations.push_back(std::move(o));
    }
    return calib::projector::calibrate(observations,{640,480},{480,270},camera_k,d,12.5);
}

void testSyntheticSolveAndFile()
{
    std::vector<calib::projector::CalibrationObservation> observations;
    const auto solved=syntheticSolve(observations);
    assert(solved.ok && cv::checkRange(solved.projector_matrix) && cv::checkRange(solved.projector_distortion));
    assert(cv::checkRange(solved.rotation_camera_to_projector) && cv::norm(solved.translation_camera_to_projector)>1.0);
    assert(cv::determinant(solved.rotation_camera_to_projector)>0.99);
    assert(solved.translation_camera_to_projector.at<double>(0)>0.0); // Camera -> Projector direction.
    const auto path=std::filesystem::temp_directory_path()/"camera_projector_contract.yml";
    std::string error;
    const cv::Mat camera_k=(cv::Mat_<double>(3,3)<<700,0,320,0,700,240,0,0,1);
    assert(calib::projector::saveCalibration(path,{640,480},{480,270},{11,12,480,270},{9,6},camera_k,
           cv::Mat::zeros(1,5,CV_64F),solved,12.5,error));
    cv::FileStorage storage(path.string(),cv::FileStorage::READ);
    assert(static_cast<int>(storage["board_corners_x"])==9 && static_cast<int>(storage["board_corners_y"])==6);
    assert(static_cast<double>(storage["square_size_mm"])==12.5);
    assert(static_cast<int>(storage["camera_width"])==640 && static_cast<int>(storage["projector_width"])==480);
    assert(static_cast<int>(storage["pattern_x"])==11 && !storage["R_camera_to_projector"].empty());
    std::filesystem::remove(path);
}

void testDatasetFailuresAndOverwrite()
{
    const auto root=std::filesystem::temp_directory_path()/"camera_projector_dataset_test";
    std::filesystem::remove_all(root); std::filesystem::create_directories(root);
    const auto output=root/"existing.yml"; std::ofstream(output)<<"keep";
    scan::dataset::ScanDatasetValidator validator;
    calib::projector::CameraProjectorCalibrationService service{validator};
    calib::projector::CalibrationConfig config{root,root/"mono.yml",output,{10,7},12.5,1,2,false};
    auto result=service.calibrate(config);
    assert(!result.ok && result.error_code=="camera_projector_output_exists");
    config.output_file=root/"new.yml";
    result=service.calibrate(config);
    assert(!result.ok && result.error_code=="camera_projector_mono_load_failed");
    std::filesystem::remove_all(root);
}

void writePose(const std::filesystem::path& pose,int index,const cv::Mat& camera_k,const cv::Mat& projector_k)
{
    const cv::Size image_size{640,480}; const cv::Size board{10,7};
    const auto source=checkerboard(board);
    std::vector<cv::Point3f> plane{{-12.5f,-12.5f,0},{125.0f,-12.5f,0},{125.0f,87.5f,0},{-12.5f,87.5f,0}};
    const cv::Mat rv=(cv::Mat_<double>(3,1)<<0.04*index,-0.06+0.025*index,0.02*index);
    const cv::Mat tv=(cv::Mat_<double>(3,1)<<-55+18*index,-40+7*index,520+55*index);
    const cv::Mat d=cv::Mat::zeros(1,5,CV_64F);
    std::vector<cv::Point2f> camera_quad; cv::projectPoints(plane,rv,tv,camera_k,d,camera_quad);
    const cv::Mat r_cp=(cv::Mat_<double>(3,3)<<0.9998,0,0.02,0,1,0,-0.02,0,0.9998);
    const cv::Mat t_cp=(cv::Mat_<double>(3,1)<<120,2,5), r_board=[](const cv::Mat& v){cv::Mat r;cv::Rodrigues(v,r);return r;}(rv);
    const cv::Mat r_projector=r_cp*r_board,t_projector=r_cp*tv+t_cp; cv::Mat rv_projector; cv::Rodrigues(r_projector,rv_projector);
    std::vector<cv::Point2f> projector_quad; cv::projectPoints(plane,rv_projector,t_projector,projector_k,d,projector_quad);
    std::vector<cv::Point2f> source_quad{{0,0},{static_cast<float>(source.cols-1),0},
        {static_cast<float>(source.cols-1),static_cast<float>(source.rows-1)},{0,static_cast<float>(source.rows-1)}};
    const cv::Mat h_camera=cv::getPerspectiveTransform(source_quad,camera_quad);
    const cv::Mat h_projector=cv::getPerspectiveTransform(source_quad,projector_quad);
    cv::Mat reference(image_size,CV_8UC1,cv::Scalar{127});
    cv::warpPerspective(source,reference,h_camera,image_size,cv::INTER_NEAREST,cv::BORDER_CONSTANT,cv::Scalar{127});
    std::filesystem::create_directories(pose/"decode"/"left"); std::filesystem::create_directories(pose/"scan"/"left");
    cv::imwrite((pose/"reference_before.png").string(),reference); cv::imwrite((pose/"reference_after.png").string(),reference);
    const cv::Mat h_pc=h_projector*h_camera.inv(); cv::Mat px(image_size,CV_32F),py(image_size,CV_32F),mask(image_size,CV_8UC1,cv::Scalar{255});
    for(int y=0;y<image_size.height;++y) for(int x=0;x<image_size.width;++x) {
        const cv::Mat q=h_pc*(cv::Mat_<double>(3,1)<<x,y,1); const double w=q.at<double>(2);
        px.at<float>(y,x)=static_cast<float>(q.at<double>(0)/w); py.at<float>(y,x)=static_cast<float>(q.at<double>(1)/w);
        if (!std::isfinite(px.at<float>(y,x)) || !std::isfinite(py.at<float>(y,x)) || px.at<float>(y,x)<0 ||
            py.at<float>(y,x)<0 || px.at<float>(y,x)>=480 || py.at<float>(y,x)>=270) mask.at<unsigned char>(y,x)=0; }
    { cv::FileStorage s((pose/"decode"/"left"/"projector_x.yml").string(),cv::FileStorage::WRITE);s<<"projector_x"<<px; }
    { cv::FileStorage s((pose/"decode"/"left"/"projector_y.yml").string(),cv::FileStorage::WRITE);s<<"projector_y"<<py; }
    cv::imwrite((pose/"decode"/"left"/"valid_mask.png").string(),mask);
    const std::string metadata="{\"scan_id\":\"pose\",\"pattern_count\":20,\"projector_width\":480,\"projector_height\":270,\"surface\":{\"pattern_x\":0,\"pattern_y\":0,\"pattern_width\":480,\"pattern_height\":270}}";
    std::ofstream(pose/"scan"/"metadata.json")<<metadata; std::ofstream(pose/"decode"/"metadata.json")<<metadata;
}

void testOfflineDatasetService()
{
    const auto root=std::filesystem::temp_directory_path()/"camera_projector_offline_service";
    std::filesystem::remove_all(root); std::filesystem::create_directories(root/"observations");
    const cv::Mat camera_k=(cv::Mat_<double>(3,3)<<700,0,320,0,700,240,0,0,1);
    const cv::Mat projector_k=(cv::Mat_<double>(3,3)<<550,0,240,0,540,135,0,0,1);
    for(int i=0;i<5;++i) writePose(root/"observations"/("pose_00"+std::to_string(i)),i,camera_k,projector_k);
    { cv::FileStorage s((root/"mono.yml").string(),cv::FileStorage::WRITE); s<<"K"<<camera_k<<"D"<<cv::Mat::zeros(1,5,CV_64F)<<"RMS"<<0.1<<"image_width"<<640<<"image_height"<<480; }
    scan::dataset::ScanDatasetValidator validator; calib::projector::CameraProjectorCalibrationService service{validator};
    calib::projector::CalibrationConfig config{root/"observations",root/"mono.yml",root/"result.yml",{10,7},12.5,0.5,1.0,false};
    auto result=service.calibrate(config);
    assert(result.ok && result.total_pose_count==5 && result.accepted_pose_count==5 && result.rejected_pose_count==0);
    assert(std::filesystem::exists(config.output_file));
    auto exists=service.calibrate(config); assert(!exists.ok && exists.error_code=="camera_projector_output_exists");
    config.overwrite=true; result=service.calibrate(config); assert(result.ok);
    cv::Mat malformed;
    { cv::FileStorage s((root/"observations"/"pose_004"/"decode"/"left"/"projector_x.yml").string(),cv::FileStorage::READ); s["projector_x"]>>malformed; }
    malformed.at<float>(240,320)=std::numeric_limits<float>::quiet_NaN();
    { cv::FileStorage s((root/"observations"/"pose_004"/"decode"/"left"/"projector_x.yml").string(),cv::FileStorage::WRITE); s<<"projector_x"<<malformed; }
    config.output_file=root/"malformed-rejected.yml"; config.overwrite=false; result=service.calibrate(config);
    assert(result.ok && result.accepted_pose_count==4 && result.rejected_pose_count==1);
    writePose(root/"observations"/"pose_004",4,camera_k,projector_k);
    const std::string changed="{\"scan_id\":\"pose\",\"pattern_count\":20,\"projector_width\":480,\"projector_height\":270,\"surface\":{\"pattern_x\":1,\"pattern_y\":0,\"pattern_width\":480,\"pattern_height\":270}}";
    std::ofstream(root/"observations"/"pose_004"/"scan"/"metadata.json") << changed;
    std::ofstream(root/"observations"/"pose_004"/"decode"/"metadata.json") << changed;
    config.output_file=root/"inconsistent.yml"; config.overwrite=false; result=service.calibrate(config);
    assert(!result.ok && result.error_code=="camera_projector_inconsistent_surface");
    if (std::getenv("TOOTH_KEEP_CP_FIXTURE")) writePose(root/"observations"/"pose_004",4,camera_k,projector_k);
    if (!std::getenv("TOOTH_KEEP_CP_FIXTURE")) std::filesystem::remove_all(root);
}
}

int main()
{
    assert(calib::projector::defaultPosePlan().size()==27);
    testObservationAndMovement();
    testSyntheticSolveAndFile();
    testDatasetFailuresAndOverwrite();
    testOfflineDatasetService();
}
