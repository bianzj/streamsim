//
// Created by bianzunjian on 2024/3/21.
//

#ifndef FIELD_RADIOSITY_GEOMETRY_H
#define FIELD_RADIOSITY_GEOMETRY_H

#include "structs.h"
#include "fileio.h"
#include "../radiosity/radiosityio.h"
#include "../radiosityeb/radiosityebio.h"

class Geometry {
public:
    Geometry()= default;


    bool createGeometry(std::shared_ptr<FileIO> &fileio, std::shared_ptr<RadiosityIO> &modelio);
    bool createGeometry(std::shared_ptr<FileIO> &fileio, std::shared_ptr<RadiosityEBIO> &modelio);
    SensorMatrix createSensor(glm::vec3 scenesize, glm::vec3 sceneorigen, float vza, float vaa, float ratio = 1.0);
    LightSet createLight(float sza, float saa, float direct, float diffuse,float,float);

    void calcSolarAngle(float lat,float lon,float t,Angle &angle,float zone=0);
    double calcSolNoon(double jd, double longitude, double timezone);
    double calcSunriseSetUTC(bool rise, double JD, double latitude, double longitude);
    // rise = 1 for sunrise, 0 for sunset
    double calcSunriseSet(bool rise, double JD, double latitude, double longitude, double timezone);
    double calcJDofNextPrevRiseSet(double next, bool rise, double JD, double latitude, double longitude, double tz);
    double calcEquationOfTime(double t);
    double radToDeg(double angleRad);
    double degToRad(double angleDeg);
    double calcGeomMeanLongSun(double t);
    double calcGeomMeanAnomalySun(double t);
    double calcEccentricityEarthOrbit(double t);
    void calcDateFromJD(double jd);
    double calcSunEqOfCenter(double t);
    double calcSunTrueLong(double t);
    double calcSunTrueAnomaly(double t);
    double calcSunRadVector(double t);
    double calcSunApparentLong(double t);
    double calcMeanObliquityOfEcliptic(double t);
    double calcObliquityCorrection(double t);
    double calcSunRtAscension(double t);
    double calcSunDeclination(double t);
    double calcHourAngleSunrise(double lat, double solarDec);
    bool isNumber(std::string inputVal);
    double calcRefraction(double elev);
    double calcTimeInMinutes(int hours, int mins, int secs);
    void calcSkyAngle(float * skyvza,float * skyvaa);


    //void calcSolarAngle(std::shared_ptr<Defined> d,std::shared_ptr<PixelIO> pixelio);


    void calcAzEl(double T, double localtime, double latitude, double longitude, double zone);

    double calcTimeJulianCent(double jd);
    double calcJDFromJulianCent(double t);
    bool isLeapYear(int year);
    double calcDoyFromJD(double jd);
    double getJD();
    int getYear();
    int getMonth();
    int getDay();

    float sza;
    float saa;
    int year;
    int month;
    int day;
};


#endif //FIELD_RADIOSITY_GEOMETRY_H
