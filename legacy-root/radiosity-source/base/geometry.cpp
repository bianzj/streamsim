//
// Created by bianzunjian on 2024/3/21.
//

#include "geometry.h"
#include "geometry.h"
/*
define the sensor
including the observation geometry, band...

*/


//Geometry::Geometry(int year, int month, int day) {
//    this->year = year;
//    this->day = day;
//    this->month = month;
//}
bool Geometry::createGeometry(std::shared_ptr<FileIO> &fileio, std::shared_ptr<RadiosityIO> &modelio){

//    auto & sceneio = modelio->m_sceneio;


    //--------------------------------------------------------
    //--- Resolution
    //-------------------------------------------------------
    modelio->imageSize = fileio->m_pRadiosityXml->sensorxml.resolution;
//    CameraManip.setWindowSize(modelio->imageSize.x, modelio->imageSize.y );
    //---------------------------------------------------------
    // Angles
    //---------------------------------------------------------
    float vza, vaa, sza, saa;
    LightXml lightxml = fileio->m_pRadiosityXml->lightxml;
    SensorXml sensorxml = fileio->m_pRadiosityXml->sensorxml;




    sza = lightxml.solarAngle[0];
    saa = lightxml.solarAngle[1];
    modelio->sza = sza;
    modelio->saa = saa;
    modelio->n_angle = sensorxml.viewAngles.size();
    for (int i = 0; i < sensorxml.viewAngles.size(); i++)
    {
        vza = sensorxml.viewAngles[i][0];
        vaa = sensorxml.viewAngles[i][1];
        modelio->angles.emplace_back(Angle{sza, saa, vza, vaa});
    }
    //modelio->n_angle = sensorxml.viewAngles.size();
    //---------------------------------------------------------
    // Bands
    //---------------------------------------------------------
    modelio->waves = fileio->m_pRadiosityXml->sensorxml.waves;
    modelio->n_wave = modelio->waves.size();
    //---------------------------------------------------------
    // LIGHT AND SENSOR INI with Angle 0 and Band 0
    //---------------------------------------------------------
    vza = modelio->angles[0].vza;
    vaa = modelio->angles[0].vaa;
    sza = modelio->angles[0].sza;
    saa = modelio->angles[0].saa;


    return true;

}

bool Geometry::createGeometry(std::shared_ptr<FileIO> &fileio, std::shared_ptr<RadiosityEBIO> &modelio){

//    auto & sceneio = modelio->m_sceneio;


    //--------------------------------------------------------
    //--- Resolution
    //-------------------------------------------------------
    modelio->imageSize = fileio->m_pRadiosityebXml->sensorxml.resolution;
//    CameraManip.setWindowSize(modelio->imageSize.x, modelio->imageSize.y );
    //---------------------------------------------------------
    // Angles
    //---------------------------------------------------------
    float vza, vaa, sza, saa;
    LightXml lightxml = fileio->m_pRadiosityebXml->lightxml;
    SensorXml sensorxml = fileio->m_pRadiosityebXml->sensorxml;

    modelio->light.skyTemperature = lightxml.skyTemperature;


    sza = lightxml.solarAngle[0];
    saa = lightxml.solarAngle[1];
    modelio->sza = sza;
    modelio->saa = saa;
    modelio->n_angle = sensorxml.viewAngles.size();
    for (int i = 0; i < sensorxml.viewAngles.size(); i++)
    {
        vza = sensorxml.viewAngles[i][0];
        vaa = sensorxml.viewAngles[i][1];
        modelio->angles.emplace_back(Angle{sza, saa, vza, vaa});
    }
    //modelio->n_angle = sensorxml.viewAngles.size();
    //---------------------------------------------------------
    // Bands
    //---------------------------------------------------------
    modelio->waves = fileio->m_pRadiosityebXml->sensorxml.waves;
    modelio->n_wave = modelio->waves.size();
    //---------------------------------------------------------
    // LIGHT AND SENSOR INI with Angle 0 and Band 0
    //---------------------------------------------------------
    vza = modelio->angles[0].vza;
    vaa = modelio->angles[0].vaa;
    sza = modelio->angles[0].sza;
    saa = modelio->angles[0].saa;


    return true;

}




double Geometry::calcTimeJulianCent(double jd) {
    double T = (jd - 2451545.0)/36525.0;
    return T;
}

double Geometry::calcJDFromJulianCent(double t) {
    double JD = t * 36525.0 + 2451545.0;
    return JD;
}

bool Geometry::isLeapYear(int year) {
    return ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0);
}

void Geometry::calcDateFromJD(double jd) {
    double z = floor(jd + 0.5);
    double f = (jd + 0.5) - z;
    double A;
    if (z < 2299161) {
        A = z;
    } else {
        double alpha = floor((z - 1867216.25)/36524.25);
        A = z + 1 + alpha - floor(alpha/4);
    }
    double B = A + 1524;
    double C = floor((B - 122.1)/365.25);
    double D = floor(365.25 * C);
    double E = floor((B - D)/30.6001);
    this->day = B - D - floor(30.6001 * E) + f;
    this->month = (E < 14) ? E - 1 : E - 13;
    this->year = (this->month > 2) ? C - 4716 : C - 4715;
    std::cout << "\n" << this->year;
    //Date date(this->year, this->month, this->day);

    //return date;
}

double Geometry::calcDoyFromJD(double jd) {
    calcDateFromJD(jd);
    std::cout << "\n" << year;
    int k = (isLeapYear(year) ? 1 : 2);
    double doy = floor((275 * month)/9.0) - k * floor((month+ 9)/12.0) + day -30;

    return doy;
}

double Geometry::getJD() {
    if (month <= 2) {
        year -= 1;
        month += 12;
    }
    double A = floor(year/100);
    double B = 2 - A + floor(A/4);
    double JD = floor(365.25*(year + 4716)) + floor(30.6001*(month+1)) + day + B - 1524.5;
    return JD;
}


double Geometry::calcSolNoon(double jd, double longitude, double timezone) {
    //SolarPositionCalculation *spc = new SolarPositionCalculation();
    double tnoon = calcTimeJulianCent(jd - longitude/360.0);
    double eqTime = calcEquationOfTime(tnoon);
    double solNoonOffset = 720.0 - (longitude * 4) - eqTime; // in minutes
    double newt = calcTimeJulianCent(jd + solNoonOffset/1440.0);
    eqTime = calcEquationOfTime(newt);
    double solNoonLocal = 720 - (longitude * 4) - eqTime + (timezone*60.0); // in minutes
    while (solNoonLocal < 0.0) {
        solNoonLocal += 1440.0;
    }
    while (solNoonLocal >= 1440.0) {
        solNoonLocal -= 1440.0;
    }
    //delete spc;
    return solNoonLocal;
}


double Geometry::calcSunriseSetUTC(bool rise, double JD, double latitude, double longitude) {

    double t = calcTimeJulianCent(JD);
    double eqTime = calcEquationOfTime(t);
    double solarDec = calcSunDeclination(t);
    double hourAngle = calcHourAngleSunrise(latitude, solarDec);
    if (!rise)
        hourAngle = -hourAngle;
    double delta = longitude + radToDeg(hourAngle);
    double timeUTC = 720 - (4.0 * delta) - eqTime;	// in minutes

    return timeUTC;
}


// rise = 1 for sunrise, 0 for sunset
double Geometry::calcSunriseSet(bool rise, double JD, double latitude, double longitude, double timezone) {

    double timeUTC = calcSunriseSetUTC(rise, JD, latitude, longitude);
    double newTimeUTC = calcSunriseSetUTC(rise, JD + timeUTC/1440.0, latitude, longitude);
    double jday;
    double azimuth;

    if (isNumber(std::to_string(newTimeUTC))) {
        double timeLocal = newTimeUTC + (timezone * 60.0);
        double riseT = calcTimeJulianCent(JD + newTimeUTC/1440.0);

        calcAzEl(riseT, timeLocal, latitude, longitude, timezone);

        //double azimuth = riseAzEl.azimuth;
        //azimuth = riseAzEl.getAzimuth(); // See TODO #1
        jday = JD;
        if ( (timeLocal < 0.0) || (timeLocal >= 1440.0) ) {
            double increment = ((timeLocal < 0) ? 1 : -1);
            while ((timeLocal < 0.0)||(timeLocal >= 1440.0)) {
                timeLocal += increment * 1440.0;
                jday -= increment;
            }
        }
        //delete calcAzEl(riseT, timeLocal, latitude, longitude, timezone);

    } else { // no sunrise/set found

        azimuth = -1.0;
        double timeLocal = 0.0;
        double doy = calcDoyFromJD(JD);
        if ( ((latitude > 66.4) && (doy > 79) && (doy < 267)) ||
             ((latitude < -66.4) && ((doy < 83) || (doy > 263))) ) {
            //previous sunrise/next sunset
            jday = calcJDofNextPrevRiseSet(!rise, rise, JD, latitude, longitude, timezone);
        } else {   //previous sunset/next sunrise
            jday = calcJDofNextPrevRiseSet(rise, rise, JD, latitude, longitude, timezone);
        }
    }
    //delete spc;
    //return {"jday": jday, "timelocal": timeLocal, "azimuth": azimuth}
    return 0.0;
}

double Geometry::calcJDofNextPrevRiseSet(double next, bool rise, double JD, double latitude, double longitude, double tz) {

    double julianday = JD;
    double increment = ((next) ? 1.0 : -1.0);
    double time = calcSunriseSetUTC(rise, julianday, latitude, longitude);

    while(!isNumber(std::to_string(time))) {
        julianday += increment;
        time = calcSunriseSetUTC(rise, julianday, latitude, longitude);
    }
    double timeLocal = time + tz * 60.0;
    while ((timeLocal < 0.0) || (timeLocal >= 1440.0)) {
        double incr = ((timeLocal < 0) ? 1 : -1);
        timeLocal += (incr * 1440.0);
        julianday -= incr;
    }

    return julianday;
}

double Geometry::calcEquationOfTime(double t) {
    double epsilon = calcObliquityCorrection(t);
    double l0 = calcGeomMeanLongSun(t); // variable is not ten but 'l'(L) zero.
    double e = calcEccentricityEarthOrbit(t);
    double m = calcGeomMeanAnomalySun(t);

    double y = tan(degToRad(epsilon)/2.0);
    y *= y;

    double sin2l0 = sin(2.0 * degToRad(l0));
    double sinm   = sin(degToRad(m));
    double cos2l0 = cos(2.0 * degToRad(l0));
    double sin4l0 = sin(4.0 * degToRad(l0));
    double sin2m  = sin(2.0 * degToRad(m));

    double Etime = y * sin2l0 - 2.0 * e * sinm + 4.0 * e * y * sinm * cos2l0 - 0.5 * y * y * sin4l0 - 1.25 * e * e * sin2m;
    return radToDeg(Etime)*4.0;	// in minutes of time
}

double Geometry::radToDeg(double angleRad) {
    return (180.0 * angleRad / PI);
}

double Geometry::degToRad(double angleDeg) {
    return (PI * angleDeg / 180.0);
}

double Geometry::calcGeomMeanLongSun(double t) {
    double L0 = 280.46646 + t * (36000.76983 + t*(0.0003032));
    while(L0 > 360.0) {
        L0 -= 360.0;
    }
    while(L0 < 0.0) {
        L0 += 360.0;
    }
    return L0; // in degrees
}

double Geometry::calcGeomMeanAnomalySun(double t) {
    double M = 357.52911 + t * (35999.05029 - 0.0001537 * t);
    return M; // in degrees
}

double Geometry::calcEccentricityEarthOrbit(double t) {
    double e = 0.016708634 - t * (0.000042037 + 0.0000001267 * t);
    return e;		// unitless
}

double Geometry::calcSunEqOfCenter(double t) {
    double m = calcGeomMeanAnomalySun(t);
    double mrad = degToRad(m);
    double sinm = sin(mrad);
    double sin2m = sin(mrad+mrad);
    double sin3m = sin(mrad+mrad+mrad);
    double C = sinm * (1.914602 - t * (0.004817 + 0.000014 * t)) + sin2m * (0.019993 - 0.000101 * t) + sin3m * 0.000289;
    return C; // in degrees
}

double Geometry::calcSunTrueLong(double t) {
    double l0 = calcGeomMeanLongSun(t);
    double c = calcSunEqOfCenter(t);
    double O = l0 + c;
    return O; // in degrees
}

double Geometry::calcSunTrueAnomaly(double t) {
    double m = calcGeomMeanAnomalySun(t);
    double c = calcSunEqOfCenter(t);
    double v = m + c;
    return v; // in degrees
}

double Geometry::calcSunRadVector(double t) {
    double v = calcSunTrueAnomaly(t);
    double e = calcEccentricityEarthOrbit(t);
    double R = (1.000001018 * (1 - e * e)) / (1 + e * cos(degToRad(v)));
    return R; // in AUs
}

double Geometry::calcSunApparentLong(double t) {
    double o = calcSunTrueLong(t);
    double omega = 125.04 - 1934.136 * t;
    double lambda = o - 0.00569 - 0.00478 * sin(degToRad(omega));
    return lambda; // in degrees
}

double Geometry::calcMeanObliquityOfEcliptic(double t) {
    double seconds = 21.448 - t*(46.8150 + t*(0.00059 - t*(0.001813)));
    double e0 = 23.0 + (26.0 + (seconds/60.0))/60.0;
    return e0; // in degrees
}

double Geometry::calcObliquityCorrection(double t) {
    double e0 = calcMeanObliquityOfEcliptic(t);
    double omega = 125.04 - 1934.136 * t;
    double e = e0 + 0.00256 * cos(degToRad(omega));
    return e; // in degrees
}

double Geometry::calcSunRtAscension(double t) {
    double e = calcObliquityCorrection(t);
    double lambda = calcSunApparentLong(t);
    double tananum = (cos(degToRad(e)) * sin(degToRad(lambda)));
    double tanadenom = (cos(degToRad(lambda)));
    double alpha = radToDeg(atan2(tananum, tanadenom));
    return alpha; // in degrees
}

double Geometry::calcSunDeclination(double t) {
    double e = calcObliquityCorrection(t);
    double lambda = calcSunApparentLong(t);
    double sint = sin(degToRad(e)) * sin(degToRad(lambda));
    double theta = radToDeg(asin(sint));
    return theta; // in degrees
}

double Geometry::calcHourAngleSunrise(double lat, double solarDec) {
    double latRad = degToRad(lat);
    double sdRad  = degToRad(solarDec);
    double HAarg = (cos(degToRad(90.833))/(cos(latRad)*cos(sdRad))-tan(latRad) * tan(sdRad));
    double HA = acos(HAarg);
    return HA; // in radians (for sunset, use -HA)
}

bool Geometry::isNumber(std::string inputVal) {
    bool oneDecimal = false;
    std::string inputStr = "" + inputVal;
    for (int i = 0; i < inputStr.length(); i++) {
        char oneChar = inputStr.at(i);
        if (i == 0 && (oneChar == '-' || oneChar == '+')) {
            continue;
        }
        if (oneChar == '.' && !oneDecimal) {
            oneDecimal = true;
            continue;
        }
        if (oneChar < '0' || oneChar > '9') {
            return false;
        }
    }
    return true;
}

double Geometry::calcRefraction(double elev) {
    double correction;
    if (elev > 85.0) {
        correction = 0.0;
    } else {
        double te = tan(degToRad(elev));
        if (elev > 5.0) {
            correction = 58.1 / te - 0.07 / (te*te*te) + 0.000086 / (te*te*te*te*te);
        } else if (elev > -0.575) {
            correction = 1735.0 + elev * (-518.2 + elev * (103.4 + elev * (-12.79 + elev * 0.711) ) );
        } else {
            correction = -20.774 / te;
        }
        correction = correction / 3600.0;
    }
    return correction;
}

void Geometry::calcAzEl(double T, double localtime, double latitude, double longitude, double zone) {

    double eqTime = calcEquationOfTime(T);
    double theta  = calcSunDeclination(T);

    double solarTimeFix = eqTime + 4.0 * longitude - 60.0 * zone;
    double earthRadVec = calcSunRadVector(T);
    double trueSolarTime = localtime + solarTimeFix;

    double azimuth;

    while (trueSolarTime > 1440) {
        trueSolarTime -= 1440;
    }
    double hourAngle = trueSolarTime / 4.0 - 180.0;
    if (hourAngle < -180) {
        hourAngle += 360.0;
    }
    double haRad = degToRad(hourAngle);
    double csz = sin(degToRad(latitude)) * sin(degToRad(theta)) + cos(degToRad(latitude)) * cos(degToRad(theta)) * cos(haRad);
    if (csz > 1.0) {
        csz = 1.0;
    } else if (csz < -1.0) {
        csz = -1.0;
    }
    double zenith = radToDeg(acos(csz));
    double azDenom = (cos(degToRad(latitude)) * sin(degToRad(zenith)));
    if (abs(azDenom) > 0.001) {
        double azRad = (( sin(degToRad(latitude)) * cos(degToRad(zenith)) ) - sin(degToRad(theta))) / azDenom;
        if (abs(azRad) > 1.0) {
            if (azRad < 0) {
                azRad = -1.0;
            } else {
                azRad = 1.0;
            }
        }
        azimuth = 180.0 - radToDeg(acos(azRad));
        if (hourAngle > 0.0) {
            azimuth = -azimuth;
        }
    } else {
        if (latitude > 0.0) {
            azimuth = 180.0;
        } else {
            azimuth = 0.0;
        }
    }
    if (azimuth < 0.0) {
        azimuth += 360.0;
    }
    double exoatmElevation = 90.0 - zenith;

    // Atmospheric Refraction correction
    double refractionCorrection = calcRefraction(exoatmElevation);

    double solarZen = zenith - refractionCorrection;
    double elevation = 90.0 - solarZen;

    //AzimuthElevation azimuth_elevation(azimuth, elevation);

    //cout << "Azimuth: " << azimuth << ", Elevation: " << elevation << endl;
    //return azimuth_elevation;
    sza = solarZen;
    saa = azimuth;
}

double Geometry::calcTimeInMinutes(int hours, int mins, int secs) {
    return (hours*60) + mins + (secs/60.0);
}


void Geometry::calcSolarAngle(float lat,float lon,float t,Angle &angle,float zone) {

    t = t - 8/24.0;
    double doy = int(t);
    double minutes = (t-int(t))*60*24.0;
    double T = t;
    calcAzEl(T,minutes,lat,lon,zone);
    angle.sza = sza;
    angle.saa = saa;
//    angle.vza = 0;
//    angle.vaa = 0;

}


void Geometry::calcSkyAngle(float *skyvza, float *skyvaa) {
    int nsky = NSKY;
    float pi =3.141592653;
    float eps=0.0001;
    float S = 2.0*pi/nsky;
    float sqS = sqrt(S);
    int k = nsky;
    float th = pi/2;
    float r,cap,the,x,nphi,xnphi,edge,cos2,th2,phi;

    while( k != 0)
    {
        r = sin(th);
        cap = 2*pi*(1-cos(th));

        if (k <= 6)
        {
            if (k == 1) th = 0;
            the = th/2;
            x = k;
            for(int j=1;j<=k;j++)
            {
                phi = (2*pi/x)*(j);
                skyvza[k-j]=the;
                skyvaa[k-j]=phi;
            }
            //         for(int i=0;i<40;i++) cout<<zenith[i]<<" "<<azimuth[i]<<endl;
            return;
        }

        nphi = int(2*pi*(cos(th-sqS)-cos(th))/S);
        if (nphi > k)    nphi = k;

        xnphi = nphi*1.0;
        edge = 2* pi*r/xnphi;
        cos2 = xnphi*S/(2*pi)+cos(th);
        th2 = acos(cos2);
        the = (th+th2)/2;
        for(int j=1;j<=nphi;j++)
        {
            phi = (2*pi/xnphi)*(j);
            skyvza[k-j]=the;
            skyvaa[k-j]=phi;
        }
        k = k - nphi;
        th = th2;
    }

}
