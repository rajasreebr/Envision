/*
This file is part of Envision.

Envision is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Envision is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Envision.  If not, see <http://www.gnu.org/licenses/>

Copywrite 2012 - Oregon State University

*/
// AltWaterMaster.cpp : Defines the initialization routines for the DLL.
//
#include "stdafx.h"
#pragma hdrstop

#include "AltWaterMaster.h"
#include "Flow.h"
#include <Maplayer.h>
#include <string.h>
#include <EnvExtension.h>
#include <VDataTable.h>
#include <UNITCONV.H>
#include "GlobalMethods.h"
#include <AlgLib\AlgLib.h>
#include <AlgLib\ap.h>
#include <GDALWrapper.h>
#include <iostream>
#include <fstream>
#include <afxtempl.h>
#include "FlowContext.h"
#include <EnvModel.h>
#include "GeoSpatialDataObj.h"

using namespace alglib_impl;


int SortWrData(const void *e0, const void *e1);


AltWaterMaster::AltWaterMaster( FlowModel *pFlowModel, WaterAllocation *pWaterAllocation)
: m_pWaterAllocation(pWaterAllocation)
, m_pFlowModel(pFlowModel)
, m_typesInUse(0)
, m_colDemand(-1)							// desired demand (m3/sec)
, m_colWaterUse(-1)						// actual use
, m_colXcoord(-1)							// WR UTM zone 10 x coordinate
, m_colYcoord(-1)							// WR UTM zone 10 y coordinate
, m_colPodID(-1)							// Water Right POD ID
, m_colPDPouID(-1)						// Water Right POU ID in POD input data file
, m_colPermitCode(-1)					// WR Permit Code http://www.oregon.gov/owrd/pages/wr/wrisuse.aspx
, m_colPodRate(-1)						// WR point of diversion max rate (m3/sec)
, m_colUseCode(-1)						// WR Use Code http://www.oregon.gov/owrd/pages/wr/wrisuse.aspx
, m_colPriorDoy(-1)						// WR priority date day of year
, m_colPriorYr(-1)						// WR priority date year
, m_colBeginDoy(-1)						// WR seasonal begin day of year
, m_colEndDoy(-1)							// WR season end day year
, m_colPouRate(-1)						// WR point of use max rate (m3/sec)
, m_colPUPouID(-1)						// WR ID POU ID in POU data input file
, m_colPouIDU_INDEX(-1)					// IDU_INDEX column in POU data input file
, m_colPouPct(-1)							// The percentage of the POU, for a SnapID, that over laps the IDU_INDEX
, m_colPouArea(-1)						// The area of the pou, for a WRID, that over laps the IDU_INDEX
, m_colPouDBNdx(-1)						// a zero based index for the POU input data file itself
, m_colPouUSECODE(-1)
, m_colPouPERMITCODE(-1)
, m_colReachComid(-1)					// WR Reach comid, relates to COMID in reach layer
, m_colIrrigate(-1)						// Economic decision to irrigat,e 0=false no irrigate, 1=true yes irrigate
, m_colAREA(-1)							// IDU column for area
, m_colUGB(-1)
, m_colPOP(-1)
, m_colGAL_CAP_DY(-1)
, m_colH2ORESIDNT(-1)
, m_colH2OINDCOMM(-1)
, m_colUnAllocatedWR(-1)				// Un-allocated water right (m3/sec)
, m_colWaterDeficit(-1)					// water deficit, from ET calculations, mm/d
, m_colRealUnSatisfiedDemand(-1)		// Real UnSatisfied Demand (m3/sec)
, m_colVirtUnSatisfiedDemand(-1)		// virtual UnSatisfied Demand (m3/sec)
, m_colUnExercisedWR(-1)				// Un-exercised WR
, m_weekOfYear(0)							// the week of the year (zero based 0-51)
, m_envTimeStep(0)						// 0 based year of run
, m_nSWLevelOneIrrWRSO(0)				// number of irrigation IDUs with level one conflict, water right shut off
, m_nSWLevelTwoIrrWRSO(0)				// number of irrigation IDUs with level two conflict, water right shut off
, m_nSWLevelOneMunWRSO(0)				// number of municipal IDUs with level one conflict, water right shut off
, m_nSWLevelTwoMunWRSO(0)				// number of municipal IDUs with level two conflict, water right shut off
, m_pouSWAreaLevelOneIrrWRSO(0.0f)	// this tracks the POU area intersecting IDU associated with level 1 surface Water Right Shut Off (SO) 
, m_pouSWAreaLevelTwoIrrWRSO(0.0f)	// this tracks the POU area intersecting IDU associated with level 2 surface Water Right Shut Off (SO)
, m_iduSWAreaLevelOneIrrWRSO(0.0f)	// this tracks the IDU area associated with level 1 surface Water Right Shut Off (SO)
, m_iduSWAreaLevelTwoIrrWRSO(0.0f)	// this tracks the IDU area associated with  level 2 surface Water Right Shut Off (SO)
, m_minFlow(0.f)							// minimum flow threshold for stream reach
, m_colMinFlow(-1)						// column number if exist for minimum stream reach flow in Stream layer
, m_colStrLayComid(-1)					// column number of COMID in Stream Layer
, m_colIDUIndex(-1)						// column number of IDU_INDEX in IDU layer
, m_colWRShortG(-1)						// WR shut off in IDU layer.  1=level 1 shutoff, 2=level 2 shutoff, 0=has not been shut off
, m_SWIrrWater(0.0f)						// daily allocated surface water irrigation m3/sec
, m_SWIrrWaterMmDy(0.0f)					// daily allocated surface water irrigation mm/day
, m_GWIrrWater(0.0f)						// daily allocated ground water irrigation m3/sec
, m_GWIrrWaterMmDy(0.0f)					// daily allocated ground water irrigation mm/day
, m_SWIrrDuty(0.0f)						// daily surface water irrigation duty m3
, m_GWIrrDuty(0.0f)						// daily ground water irrigation duty m3
, m_idusPerPou(0)						   // the number of IDUs per WR POU
, m_remainingIrrRequest(0.0f)        // as multiple PODs satisfy Irrigation Request in IDU this is what is remaining
, m_SWIrrAreaYr(0.0f)						// daily area of surface water irrigation
, m_SWIrrAreaDy(0.0f)                // daily area of irrigated IDUs (acres)
, m_GWIrrAreaDy(0.0f)                // daily area of irrigated IDUs (acres)
, m_SWMuniWater(0.0f)						// daily allocated municipal water
, m_SWunallocatedIrrWater(0.0f)      // daily unallocated ag water
, m_SWunsatIrrWaterDemand(0.0f)      // daily unsatisfied ag water demand
, m_GWUnSatIrr(0.0f)						// daily unsatisfied irrigation from ground water m3/sec
, m_SWUnSatIrr(0.0f)						// daily unsatisfied irrigation from stream m3/sec
, m_SWUnExIrr(0.0f)						// daily unexercised irrigation from surface water m3/sec
, m_SWUnExMun(0.0f)						// daily unexercised municipal from surface water m3/sec
, m_GWUnExIrr(0.0f)						// daily unexercised irrigation from ground water m3/sec
, m_SWUnAllocIrr(0.0f)					// daily unallocated irrigatin from surface water m3/sec 
, m_GWUnAllocIrr(0.0f)					// daily unallocated irrigatin from ground water m3/sec
, m_regulatoryDemand(0.0f)           // daily instream water right demand m3/sec 
, m_inStreamUseCnt(0)					// instream water right diversion count
, m_colAllocatedIrrigation(-1)      // irrigation allocation
, m_colDemandFraction(-1)
, m_wrIrrigateFlag(true)				// for each flow time step if right exist and demand is zero, flag is false. default true.
, m_demandIrrigateFlag(true)			// If economic to irrigate at envision time step is 1, then true. elseif  0 then false.
, m_colUDMAND_DY(-1)			// Daily Urban water demand from the IDU layer m3/sec
, m_colPlantDate(-1)						// Crop planting date in IDU layer
, m_colHarvDate(-1)					   // Crop harvest date in IDU layer
, m_colMunPodRate(-1)               // total POD rate for all PODs in IDU with municipal water right m3/sec
, m_irrDefaultBeginDoy(-1)			   // The default irrigation begin Day of Year if not specified in Water Right. Set in .xml file (1 base)
, m_irrDefaultEndDoy(-1)				// The default irrigation end Day of Year if not specified in Water Right.  Set in .xml file (1 base)
, m_SWCorrFactor(0.0f)				   // correction factor for surface water POD rate diversion (unitless)
, m_GWCorrFactor(0.0f)				   //correction factor for ground water POD rate diversion (unitless)
, m_colSWIrrDy(-1)						// Daily surface water applied to IDU (mm/day)
, m_colGWIrrDy(-1)						// Daily ground water applied to IDU (mm/day)
, m_areaDutyExceeds(0.0f)				// sum area that have been shut off after WR exceeds it's allowed maximum
, m_demandDutyExceeds(0.0f) 			// acre-ft of demand not satisfied when irrigated land exceeding duty threshold
, m_demandOutsideBegEndDates(0.0f)  // acre-ft of demand requested outside of regulatory begin/end diversion dates 
, m_colIrrApplied(-1)
, m_colWRID(-1)
, m_colLulc_A(-1)							// Land Use/Land Cover (coarse). used for finding Agriculture IDUs
, m_colMaxDutyFile(-1)              // For irrigatable IDUs, if this attribute present in POD input data set, is  maximum duty for IDU
, m_colMaxDutyLayer(-1)					// For irrigatable IDUs, if this attribute present in IDU layer, is  maximum duty for IDU
, m_maxDuty(100000.0f)					// For irrigatable IDUs, maximum duty, if present in POD data set or IDU layer, else set with default in .xml file 
, m_fractionDischargeAvail(0.7f)    //// this value is multiplied against the discharge in a reach to determine water available to water rights, set in .xml file
, m_maxRate(0.0f)							// For irrigatable IDUs, maximum rate 
, m_maxIrrDiversion(0.0f)				// The maximum irrigation diversion allowed per day mm/day
, m_SWIrrUnSatDemand(0.0f)          // if available surface water is less than half irrigation request m3/sec
, m_SWMunUnSatDemand(0.0f)          // if available surface water is less than half municipal demand m3/sec
, m_GWIrrUnSatDemand(0.0f)          // if available ground water is less than half irrigation request m3/sec
, m_GWMunUnSatDemand(0.0f)          // if available ground water is less than half municipal demand m3/sec
, m_colWRConflictYr(-1)             // Attribute in stream layer that types a reach as being in "conflict" (not satisfying WR) 0- no conflict, 1-available < demand, 2-available < demand/2
, m_colWRConflictDy(-1)             // Attribute in stream layer that types a reach as being in "conflict" (not satisfying WR) 0- no conflict, 1-level one, 2-level two
, m_irrWaterRequestYr(0.0f) 			// Total Irrgation Request (acre-ft per year)
, m_irrigableIrrWaterRequestYr(0.0f)// Irrigable Irrigation Request (acre-ft per year)
, m_irrigatedWaterYr(0.0f)				// Allocated irrigation Water (acre-ft per year)
, m_irrigatedSurfaceYr(0.0f)			// Allocated surface water for irrigation (acre-ft per year)
, m_irrigatedGroundYr(0.0f)			// Allocated ground water for irrigation (acre-ft per year)
, m_unSatisfiedIrrigationYr(0.0f)	// Unsatisified Irrigation Water (acre-ft per year)
, m_wastedWaterRateDy(0.0f)         // wasted water, see watermaster.h
, m_wastedWaterVolYr(0.0f)          // wasted water, see watermaster.h
, m_exceededWaterRateDy(0.0f)
, m_exceededWaterVolYr(0.0f)
, m_daysPerYrMaxRateExceeded( 0.0f)
, m_colIrrRequestYr(-1)					// Annual Irrigation Request column in IDU layer (acre-ft per year)
, m_colIrrRequestDy(-1)					// Daily Irrigation Request column in IDU layer (acre-ft per year)
, m_colMaxTotDailyIrr(-1) 			   // column in IDU layer for Maximum total daily applied irrigation water (acre-ft)
, m_colaveTotDailyIrr(-1) 		      // column in IDU layer for average total daily applied irrigation water (acre-ft)
, m_GWMuniWater(0.0f)				   // daily allocated municipal water from well m3/sec
, m_IrrWaterRequestDy(0.0f)         // Total daily irrigation request (mm per day)
, m_colReachLength(-1)              // column number of the attrigute "LENGTH" in the stream layer
, m_basinReachLength(0.0f)				// total lenght of stream reaches in the study area
, m_irrLenWtReachConflictYr(0.0f)   // if reach is in conflict, annual total of reach lenght/basin reach length
, m_colNInConflict(-1)					// column number in stream layer for number of days reach in conflict
, m_colPodUseRate(-1)  				   // column for pod use rate m3/sec
, m_exportDistPodComid(0)				// If set equal to 1, export .csv file with distances between stream centroid and POD xy coordinates
, m_colMUNIALLO_D(-1)		// column in IDU layer for daily municipal water allocations (m3/sec)
, m_maxDistanceToReach(1000000)     // the maximum distance between pod and reach before a water right is exercised (m)
, m_colDistPodReach(-1)					// column for pod to reach distance (m)
, m_irrigateDecision(1)					// decision to irrigate 1-yes, 0-no.  default yes
, m_maxDutyHalt(0)						// If 1-yes then halt irrigation to IDU if maximum duty is reached
, m_colIrrUseOrCancel(-1)           // column in IDU layer that 1=yes Water right canceled, 0-no water right active. for irrigation
, m_colMunUseOrCancel(-1)           // column in IDU layer that 1=yes Water right canceled, 0-no water right active. for Municipal
, m_colWRShutOffMun(-1)             // WR municipal shut off in IDU layer.  1=level 1 shutoff, 2=level 2 shutoff, 0=has not been shut off
, m_debug(0)								// if 1-yes then will invoke helpful debugging stuff
, m_dyGTmaxPodArea21(0.0f)				// vineyards and tree farms area > maxPOD(acres)"); //21
, m_dyGTmaxPodArea22(0.0f)				// Grass seed area > maxPOD(acres)"); //22
, m_dyGTmaxPodArea23(0.0f)				// Pasture area > maxPOD(acres)"); //23
, m_dyGTmaxPodArea24(0.0f)				// Wheat area > maxPOD(acres)"); //24
, m_dyGTmaxPodArea25(0.0f)				// Fallow area > maxPOD(acres)"); //25
, m_dyGTmaxPodArea26(0.0f)				// Corn area > maxPOD(acres)"); //26
, m_dyGTmaxPodArea27(0.0f)				// Clover area > maxPOD(acres)"); //27
, m_dyGTmaxPodArea28(0.0f)				// Hay area > maxPOD(acres)"); //28
, m_dyGTmaxPodArea29(0.0f)				// Other crops area > maxPOD(acres)"); //29	
, m_anGTmaxDutyArea21(0.0f)			// vineyards and tree farms area > maxDuty(acres)"); //21
, m_anGTmaxDutyArea22(0.0f)			// Grass seed area > maxDuty(acres)"); //22
, m_anGTmaxDutyArea23(0.0f)			// Pasture area > maxDuty(acres)"); //23
, m_anGTmaxDutyArea24(0.0f)			// Wheat area > maxDuty(acres)"); //24
, m_anGTmaxDutyArea25(0.0f)			// Fallow area > maxDuty(acres)"); //25
, m_anGTmaxDutyArea26(0.0f)			// Corn area > maxDuty(acres)"); //26
, m_anGTmaxDutyArea27(0.0f)			// Clover area > maxDuty(acres)"); //27
, m_anGTmaxDutyArea28(0.0f)			// Hay area > maxDuty(acres)"); //28
, m_anGTmaxDutyArea29(0.0f)			// Other crops area > maxDuty(acres)"); //29	
, m_colLulc_B(-1)						   // Land Use/Land Cover (medium). used for finding crop type
, m_colDailyAllocatedIrrigation(-1)	// Water Allocated for irrigation use (mm per day)
, m_colUnSatIrrRequest(-1)          // Unsatisfied irrigation request (mm per day)
, m_colActualIrrRequestDy(-1)       // Daily Actual irrigation request ( mm per day )
, m_IrrFromAllocationArrayDy(0.0f)	// check to see if array used to flux is same as values written to map ( mm/day ) action item remove after development
, m_IrrFromAllocationArrayDyBeginStep(0.0f) // check to see if array used to flux is same as values written to map ( mm/day ) action item to eventually remove
, m_pastureIDUGTmaxDuty(0)          // a pasture IDU_INDEX where max duty is exceeded
, m_pastureIDUGTmaxDutyArea(0.0f)   // a pasture area of IDU_INDEX where max duty is exceeded (m2)
, m_unSatInstreamDemand(0.0f)			// if the available source flow is less than regulatory in-stream demand in a reach, accumlate the difference (m3/sec)
, m_colSWAlloMunicipalDay(-1)       // column in IDU layer for daily municipal surface water allocations (m3/sec)
, m_colSWMUNALL_Y(-1)               // column in IDU layer for annual municipal surface water allocations (m3 H2O)
, m_colGWAlloMunicipalDay(-1)       // column in IDU layer for daily municipal ground water allocations (m3/sec)
, m_colGWMUNALL_Y(-1)               // column in IDU layer for annual municipal ground water allocations (m3 H2O)
, m_colSWUnexIrr(-1)						// when appropriated surface water POD rate is greater than request, POD rate - demand (m3/sec)
, m_colGWUnexIrr(-1)						// when appropriated ground water POD rate is greater than request, POD rate - demand (m3/sec)
, m_colSWUnexMun(-1) 					// when appropriated municipal surface water POD rate is greater than request, POD rate - demand (m3/sec)
, m_colSWUnSatMunDemandDy(-1)			// daily unsatisfied municipal demand (m3/sec)
, m_colSWUnSatIrrRequestDy(-1)      // daily surface water unsatisfied irrigation request (m3/sec)
, m_colWastedIrrDy(-1)					// 0.0125 cfs - daily allocated irrigation water, summed over irrigated IDUs (m3/sec)
, m_colExcessIrrDy(-1)					// (m3/sec)
, m_colAllocatedIrrigationAf(-1)			// Water Allocated for irrigation use (acre-ft per year)
, m_colSWAllocatedIrrigationAf(-1)  // Surface Water Allocated for irrigation use (acre-ft per year)
, m_colIRGWAF_Y(-1)  // Ground Water Allocated for irrigation use (acre-ft per year)
, m_colUnsatIrrigationAf(-1)			// Unsatisfied irrigation request (acre-ft per year) 
, m_colSWUnsatIrrigationAf(-1)		// Unsatisfied surface irrigation request (acre-ft per year)
, m_colGWUnsatIrrigationAf(-1)		// Unsatisfied ground irrigation request (acre-ft per year)
, m_colWastedIrrYr(-1)              // 0.0125 cfs - annual allocated irrigation water, summed over irrigated IDUs (m3/sec)
, m_colExcessIrrYr(-1)					// (m3/sec)
, m_colIrrExceedMaxDutyYr(-1)       // 1- yes, IDU exceeded maximum annual irrigation duty, 0-no
, m_colGWUnSatIrrRequestDy(-1)		// daily ground water unsatisfied irrigation request (m3/sec)
, m_colGWUnSatMunDemandDy(-1)			// daily ground water unsatisfied municipal demand (m3/sec)
, m_mostJuniorWR(0)						// the year of the most junior water right
, m_mostSeniorWR(0)						// the year of the most senior water right
, m_colDynamTimeStep(-1)					// column for time step specified in dynamic WR input file
, m_colDynamPermitCode(-1)				// column for permit code specified in dynamic WR input file
, m_colDynamUseCode(-1)					// column for use code specified in dynamic WR input file
, m_colDynamIsLease(-1)					// column if water right is a lease specified in dynamic WR input file
, m_DynamicWRs(false)				   // a dynamic water rights input file has loaded
, m_colWRZone(-1) 				      // column number of WRZONE in IDU layer
, m_colDSReservoir(-1)              // 1- yes, 0-no reach is downstream from reservoir
, m_coliDULayComid(-1)					// column ID of COMID in IDU layer
, m_dynamicWRType(0)						// input variable index
, m_dynamicWRRadius(1000000)			// the maximum radius from a reach to consider adding a dynamic water right (m). default is a larger number. set in .xml
, m_maxDaysShortage(365)				// the maximum number of shortage days before a Water Right is canceled for the growing season.
, m_recursiveDepth(1)					// in times of shortage, the recursive depth of upstream reaches to possibly suspend junior water rights
, m_pctIDUPOUIntersection(60.0)     // percentage of an IDU that is intersected by an water right's place of use (%)
, m_regulationType(0)               // in times of shortage, index of type of regulation 0 - no regulation, >0 see manual regulation types
, m_colWRJuniorAction(-1)           // some action is begin taken on a junion water right, depends on type of action
, m_colWRShutOff(-1)                // WR irrigation unsatisfied irrigaton request in IDU layer.  1=level, 2=level , 0=request met
, m_dynamicWRAppropriationDate(-1)  // Dynamic water right appropriation date.  If set to -1, then equals year of run
, m_colWR_MUNI(-1)
, m_colWR_INSTRM(-1)
, m_colWR_IRRIG_S(-1)
, m_colWR_IRRIG_G(-1)
, m_colSTRM_ORDER(-1)
, m_colIDUH2O_EST(-1)
, m_colIRR_STATE(-1)
, m_colPRECIP_YR(-1)
, m_colET_YR(-1)
, m_colHRU_ID(-1)
, m_basin_discharge_accumulator_m3(0.f)
, m_pouDb(U_UNDEFINED)
, m_podDb(U_UNDEFINED)
, m_dynamicWRDb(U_UNDEFINED)
, m_timeSeriesUnallUnsatIrrSummaries(U_DAYS)
, m_timeSeriesSWIrrSummaries(U_DAYS)
, m_timeSeriesSWIrrAreaSummaries(U_DAYS)
, m_timeSeriesSWMuniSummaries(U_DAYS)
, m_timeSeriesGWIrrSummaries(U_DAYS)
, m_timeSeriesGWIrrAreaSummaries(U_DAYS)
, m_timeSeriesGWMuniSummaries(U_DAYS)
, m_dailyMetrics(U_DAYS)
, m_annualMetrics(U_YEARS)
, m_dailyMetricsDebug(U_DAYS)
, m_annualMetricsDebug(U_YEARS)
, m_QuickCheckMetrics(U_YEARS)
{ 
m_pFlowModel->AddInputVar( "Dynamic Water Right type", m_dynamicWRType, "0=default, 1=allAgBelowReservoir " );
m_pFlowModel->AddInputVar( "Regulation type", m_regulationType, "0=default, 1=suspendJuniors" ); 
}

AltWaterMaster::~AltWaterMaster(void)
{

}

bool AltWaterMaster::Init(FlowContext *pFlowContext)
   {
	Report::Log("Initializing AltWaterMaster");

	MapLayer *pLayer = (MapLayer*)pFlowContext->pEnvContext->pMapLayer;

	MapLayer* pStreamLayer = (MapLayer*)pFlowContext->pFlowModel->m_pStreamLayer;
	
	// Irrigation attributes in IDU layer
	pLayer->CheckCol( m_colUnSatIrrRequest, "UNSATIRQ_D", TYPE_FLOAT, CC_MUST_EXIST );
	pLayer->CheckCol( m_colAllocatedIrrigation, "IRRALLO_Y", TYPE_FLOAT, CC_MUST_EXIST );
	pLayer->CheckCol( m_colAllocatedIrrigationAf, "IRRALAF_Y", TYPE_FLOAT, CC_MUST_EXIST );
	pLayer->CheckCol( m_colSWAllocatedIrrigationAf, "IRSWAF_Y", TYPE_FLOAT, CC_MUST_EXIST );
	pLayer->CheckCol( m_colIRGWAF_Y, "IRGWAF_Y", TYPE_FLOAT, CC_MUST_EXIST );
	
	pLayer->CheckCol( m_colUnsatIrrigationAf,   "USIRAF_Y", TYPE_FLOAT, CC_MUST_EXIST );
	pLayer->CheckCol( m_colSWUnsatIrrigationAf, "USIRSWAF_Y", TYPE_FLOAT, CC_MUST_EXIST );
	pLayer->CheckCol( m_colGWUnsatIrrigationAf, "USIRGWAF_Y", TYPE_FLOAT, CC_MUST_EXIST );

	pLayer->CheckCol( m_colDailyAllocatedIrrigation, "IRRALLO_D", TYPE_FLOAT, CC_MUST_EXIST );
	pLayer->CheckCol( m_colSWIrrDy,"SWIRR_DY", TYPE_FLOAT, CC_MUST_EXIST );
	pLayer->CheckCol( m_colGWIrrDy,"GWIRR_DY", TYPE_FLOAT, CC_MUST_EXIST );
	pLayer->CheckCol( m_colMaxTotDailyIrr, "MAXIRR_D", TYPE_FLOAT, CC_MUST_EXIST );
	pLayer->CheckCol( m_colaveTotDailyIrr, "AVEIRR_D", TYPE_FLOAT, CC_MUST_EXIST );
	pLayer->CheckCol( m_colSWUnexIrr, "SWUXIRR_DY", TYPE_FLOAT, CC_MUST_EXIST );
	pLayer->CheckCol( m_colSWUnSatIrrRequestDy, "UNSWIRQ_D", TYPE_FLOAT, CC_MUST_EXIST );
	pLayer->CheckCol( m_colGWUnSatIrrRequestDy, "UNGWIRQ_D", TYPE_FLOAT, CC_MUST_EXIST );
	pLayer->CheckCol( m_colWastedIrrDy, "WASTEIRR_D" , TYPE_FLOAT, CC_MUST_EXIST );
	pLayer->CheckCol( m_colWastedIrrYr, "WASTEIRR_Y" , TYPE_FLOAT, CC_MUST_EXIST );					
	pLayer->CheckCol( m_colExcessIrrDy, "EXCESIRR_D" , TYPE_FLOAT, CC_MUST_EXIST );
	pLayer->CheckCol( m_colExcessIrrYr, "EXCESIRR_Y" , TYPE_FLOAT, CC_MUST_EXIST );
	pLayer->CheckCol( m_colIrrExceedMaxDutyYr, "EXCEDDTY_Y" , TYPE_FLOAT, CC_MUST_EXIST ); 

	// Irrigation request attributes in IDU layer
	pLayer->CheckCol( m_colIrrRequestYr, "IRRRQST_Y", TYPE_FLOAT, CC_MUST_EXIST );
	pLayer->CheckCol( m_colIrrRequestDy, "IRRRQST_D", TYPE_FLOAT, CC_MUST_EXIST );
	pLayer->CheckCol( m_colActualIrrRequestDy, "IRRACRQ_D", TYPE_FLOAT, CC_MUST_EXIST );
	
	// Municipal attributes in the IDU layer
	pLayer->CheckCol( m_colMunPodRate, "MUNPODRATE", TYPE_FLOAT, CC_MUST_EXIST );
   pLayer->CheckCol(m_colMUNIALLO_D, "MUNIALLO_D", TYPE_FLOAT, CC_MUST_EXIST);
	pLayer->CheckCol( m_colSWAlloMunicipalDay, "SWMUNALL_D" , TYPE_FLOAT, CC_MUST_EXIST );
   pLayer->CheckCol(m_colSWMUNALL_Y, "SWMUNALL_Y", TYPE_FLOAT, CC_AUTOADD);
   pLayer->CheckCol(m_colGWAlloMunicipalDay, "GWMUNALL_D", TYPE_FLOAT, CC_MUST_EXIST);
   pLayer->CheckCol(m_colGWMUNALL_Y, "GWMUNALL_Y", TYPE_FLOAT, CC_AUTOADD);
   pLayer->CheckCol(m_colSWUnexMun, "SWUXMUN_DY", TYPE_FLOAT, CC_MUST_EXIST);
	pLayer->CheckCol( m_colSWUnSatMunDemandDy, "UNSWMDMD_D", TYPE_FLOAT, CC_MUST_EXIST );
	pLayer->CheckCol( m_colGWUnSatMunDemandDy, "UNGWMDMD_D", TYPE_FLOAT, CC_MUST_EXIST );

	pLayer->CheckCol(m_colUDMAND_DY, "UDMAND_DY", TYPE_FLOAT, CC_MUST_EXIST);

	// Scarcity attributes in the IDU and Stream layer
	pStreamLayer->CheckCol(m_colWRConflictDy, "CONFLCTDy", TYPE_INT, CC_AUTOADD);
	pStreamLayer->CheckCol(m_colWRConflictYr, "CONFLCTYr", TYPE_INT, CC_AUTOADD);
	pStreamLayer->CheckCol(m_colNInConflict, "NCONFLT", TYPE_INT, CC_AUTOADD);
	pLayer->CheckCol(m_colWRShutOffMun, "SHUTOFFMUN", TYPE_INT, CC_MUST_EXIST);
	pLayer->CheckCol(m_colWRShortG, "WR_SHORTG", TYPE_INT, CC_AUTOADD);
	pLayer->CheckCol(m_colWRShutOff, "WR_SHUTOFF", TYPE_INT, CC_AUTOADD);
   pLayer->CheckCol(m_colWRJuniorAction, "WR_JRNOTES", TYPE_INT, CC_AUTOADD);
   pLayer->CheckCol(m_colUnAllocatedWR, "UNALLOCWR", TYPE_FLOAT, CC_MUST_EXIST);
	pLayer->CheckCol(m_colIrrUseOrCancel, "IRRWRCANCL", TYPE_INT, CC_MUST_EXIST);
	pLayer->CheckCol(m_colMunUseOrCancel, "MUNWRCANCL", TYPE_INT, CC_MUST_EXIST);
	pStreamLayer->CheckCol( m_colDSReservoir, "DS_RES", TYPE_INT, CC_MUST_EXIST);            
	pLayer->CheckCol(m_coliDULayComid, "COMID", TYPE_INT, CC_MUST_EXIST);
	// Other attributes in the IDU and stream layers used in the AltWaterMaster Method
	pLayer->CheckCol(m_colIDUIndex, "IDU_INDEX", TYPE_INT, CC_MUST_EXIST);
	pLayer->CheckCol(m_colAREA, "AREA", TYPE_FLOAT, CC_MUST_EXIST);	
   pLayer->CheckCol(m_colUGB, "UGB", TYPE_INT, CC_MUST_EXIST);
   pLayer->CheckCol(m_colPOP, "POP", TYPE_FLOAT, CC_MUST_EXIST);
   pLayer->CheckCol(m_colGAL_CAP_DY, "GAL_CAP_DY", TYPE_FLOAT, CC_AUTOADD);
   pLayer->CheckCol(m_colH2ORESIDNT, "H2ORESIDNT", TYPE_FLOAT, CC_MUST_EXIST);
   pLayer->CheckCol(m_colH2OINDCOMM, "H2OINDCOMM", TYPE_FLOAT, CC_MUST_EXIST);
   pLayer->CheckCol(m_colWaterDeficit, "DEFICIT", TYPE_FLOAT, CC_MUST_EXIST);
	pLayer->CheckCol(m_colDemandFraction, "DemandFrac", TYPE_FLOAT, CC_MUST_EXIST);	
   pLayer->CheckCol(m_colPlantDate,   "PLANTDATE", TYPE_INT, CC_MUST_EXIST);
   pLayer->CheckCol(m_colHarvDate,    "HARVDATE", TYPE_INT, CC_MUST_EXIST);
	pLayer->CheckCol(m_colLulc_A, "LULC_A", TYPE_LONG, CC_MUST_EXIST);
	pLayer->CheckCol(m_colLulc_B, "LULC_B", TYPE_LONG, CC_MUST_EXIST);
   pLayer->CheckCol( m_colIrrig_yr, "Irrig_yr", TYPE_FLOAT, CC_MUST_EXIST);	
   pLayer->CheckCol(m_colWR_MUNI, "WR_MUNI", TYPE_INT, CC_AUTOADD);
   pLayer->CheckCol(m_colWR_INSTRM, "WR_INSTRM", TYPE_INT, CC_AUTOADD);
   pLayer->CheckCol(m_colWR_IRRIG_S, "WR_IRRIG_S", TYPE_INT, CC_AUTOADD);
   pLayer->CheckCol(m_colWR_IRRIG_G, "WR_IRRIG_G", TYPE_INT, CC_AUTOADD);
   pLayer->CheckCol(m_colSTRM_ORDER, "STRM_ORDER", TYPE_INT, CC_AUTOADD);
   pLayer->CheckCol(m_colIDUH2O_EST, "IDUH2O_EST", TYPE_FLOAT, CC_AUTOADD);
   pLayer->CheckCol(m_colIRR_STATE, "IRR_STATE", TYPE_INT, CC_AUTOADD);
   pLayer->CheckCol(m_colPRECIP_YR, "PRECIP_YR", TYPE_FLOAT, CC_MUST_EXIST);
   pLayer->CheckCol(m_colET_YR, "ET_YR", TYPE_FLOAT, CC_MUST_EXIST);
   pLayer->CheckCol(m_colHRU_ID, "HRU_ID", TYPE_INT, CC_AUTOADD);

   pStreamLayer->CheckCol(m_colStrLayComid, "COMID", TYPE_INT, CC_MUST_EXIST);
   pStreamLayer->CheckCol(m_colReachLength, "LENGTH", TYPE_FLOAT, CC_MUST_EXIST);

	// column names specified in .xml file, may or maynot exist in IDU or Stream Layer
   // pLayer->CheckCol(m_colWREXISTS, "WREXISTS", TYPE_INT, CC_MUST_EXIST);
   m_colWREXISTS = pLayer->GetFieldCol(m_wrExistsColName);

	m_colMinFlow = pStreamLayer->GetFieldCol(m_minFlowColName);
	m_colIrrigate = pLayer->GetFieldCol(m_irrigateColName);
	m_colMaxDutyLayer = pLayer->GetFieldCol(m_maxDutyColName); 

   int num_PODrecords = LoadWRDatabase(pFlowContext);
   if (num_PODrecords <= 0)
      {
      CString msg;
      msg.Format("*** AltWaterMaster::Init() num_PODrecords = %d", num_PODrecords);
      Report::Log(msg);
      return(false);
      }

	int iduCount = pLayer->GetRecordCount();
	int reachCount = pFlowContext->pFlowModel->GetReachCount();

   m_iduConsecutiveShortages.SetSize(iduCount);

   // conflict data obj
	m_iduSWIrrWRSOLastWeek.SetSize(iduCount);
	m_iduSWMunWRSOLastWeek.SetSize(iduCount);
	m_iduSWLevelOneIrrWRSOAreaArray.SetSize(iduCount);
	m_iduSWLevelOneMunWRSOAreaArray.SetSize(iduCount);
	m_pouSWLevelOneIrrWRSOAreaArray.SetSize(iduCount);
	m_pouSWLevelOneMunWRSOAreaArray.SetSize(iduCount);
	m_iduSWLevelTwoIrrWRSOAreaArray.SetSize(iduCount);
	m_iduSWLevelTwoMunWRSOAreaArray.SetSize(iduCount);
	m_pouSWLevelTwoIrrWRSOAreaArray.SetSize(iduCount);
	m_pouSWLevelTwoMunWRSOAreaArray.SetSize(iduCount);
	m_iduSWIrrWRSOIndex.SetSize(iduCount);
	m_iduSWMunWRSOIndex.SetSize(iduCount);
	m_iduSWIrrWRSOWeek.SetSize(iduCount);
	m_iduSWMunWRSOWeek.SetSize(iduCount);
	m_iduLocalIrrRequestArray.SetSize(iduCount);
	m_iduIsIrrOutSeasonDy.SetSize(iduCount);
	m_iduActualIrrRequestDy.SetSize(iduCount);
	m_iduWastedIrr_Dy.SetSize(iduCount);	
	m_iduExceededIrr_Dy.SetSize(iduCount);

   m_iduWR.SetSize(iduCount);

	m_pFlowModel->AddOutputVar("number of IDUs with Water Right Shut Off level one", m_nSWLevelOneIrrWRSO, "Number of IDUs with Water Right conflict level one");
	m_pFlowModel->AddOutputVar("number of IDUs with Water Right Shut Off level two", m_nSWLevelTwoIrrWRSO, "Number of IDUs with Water Right conflict level two");
	m_pFlowModel->AddOutputVar("Area of POUs with Water Right Shut Off level one (acres)", m_pouSWAreaLevelOneIrrWRSO, "Area of POUs with Water Right conflict level one (acres)");
	m_pFlowModel->AddOutputVar("Area of POUs with Water Right Shut Off level two (acres)", m_pouSWAreaLevelTwoIrrWRSO, "Area of POUs with Water Right conflict level two (acres)");
	m_pFlowModel->AddOutputVar("Area of IDUs with Water Right Shut Off level one (acres)", m_iduSWAreaLevelOneIrrWRSO, "Area of IDUs with Water Right conflict level one (acres)");
	m_pFlowModel->AddOutputVar("Area of IDUs with Water Right Shut Off level two (acres)", m_iduSWAreaLevelTwoIrrWRSO, "Area of IDUs with Water Right conflict level two (acres)");
	m_pFlowModel->AddOutputVar("Area-weighted Days/Year Max Rate Exceeded (days)", m_daysPerYrMaxRateExceeded, "");
	
	// surface water irrigation data obj
	m_iduSWUnAllocatedArray.SetSize(iduCount);
	m_iduSWIrrArrayDy.SetSize(iduCount);
	m_iduSWIrrArrayYr.SetSize(iduCount);
	m_iduSWMuniArrayDy.SetSize(iduCount);
	m_iduSWMuniArrayYr.SetSize(iduCount);
   m_iduUnsatIrrReqst_Yr.SetSize(iduCount);
	m_iduSWUnsatMunDmdYr.SetSize(iduCount);
	m_iduSWAppUnSatDemandArray.SetSize(iduCount);
	m_iduSWUnExerIrrArrayDy.SetSize(iduCount);
	m_iduSWUnExerMunArrayDy.SetSize(iduCount);
	m_iduAnnualIrrigationDutyArray.SetSize(iduCount);
	m_maxTotDailyIrr.SetSize(iduCount);
	m_aveTotDailyIrr.SetSize(iduCount);
	m_nIrrPODsPerIDUYr.SetSize(iduCount);
	m_iduExceedDutyLog.SetSize(iduCount);
	m_iduSWUnSatIrrArrayDy.SetSize(iduCount);
	m_iduSWUnSatMunArrayDy.SetSize(iduCount);	//m_iduGWUnSatIrrArrayDy.SetSize(iduCount);
	m_iduGWUnSatMunArrayDy.SetSize(iduCount);
	m_iduWastedIrr_Yr.SetSize(iduCount);
	m_iduExceededIrr_Yr.SetSize(iduCount);

	m_pFlowModel->AddOutputVar("Area of surface water irrigation (acres)", m_SWIrrAreaYr, "Area of surface water irrigation (acres)");

	// Ground water irrigation data obj
	m_iduGWUnAllocatedArray.SetSize(iduCount);
	m_iduGWIrrArrayDy.SetSize(iduCount);
	m_iduGWIrrArrayYr.SetSize(iduCount);
	m_iduGWMuniArrayDy.SetSize(iduCount);
	m_iduGWMuniArrayYr.SetSize(iduCount);
	m_iduGWUnsatIrrReqYr.SetSize(iduCount);
	m_iduGWUnsatMunDmdYr.SetSize(iduCount);
	m_iduGWAppUnSatDemandArray.SetSize(iduCount);
	m_iduGWUnExerIrrArrayDy.SetSize(iduCount);
	m_iduGWUnExerMunArrayDy.SetSize(iduCount);
	m_iduIrrWaterRequestYr.SetSize(iduCount);

	// Stream Layer arrays
	m_reachDaysInConflict.SetSize(reachCount);
	m_dailyConflictCnt.SetSize(reachCount);

	m_pFlowModel->AddOutputVar("Area of ground water irrigation (acres)", m_GWIrrAreaYr, "Area of ground water irrigation (acres)");

	//AltWaterMaster Daily Metrics
	this->m_dailyMetrics.SetName("AltWaterMaster Daily Metrics");
	this->m_dailyMetrics.SetSize(21, 0);
	this->m_dailyMetrics.SetLabel(0, "Time (day)");
	this->m_dailyMetrics.SetLabel(1, "Instream regulatory use (m3 per sec per day)"); // m_regulatoryDemand
	this->m_dailyMetrics.SetLabel(2, "Allocated surface irrigation water (m3 per sec per day)"); //m_SWIrrWater
	this->m_dailyMetrics.SetLabel(3, "Allocated ground irrigation water (m3 per sec per day)"); //m_GWIrrWater
	this->m_dailyMetrics.SetLabel(4, "Allocated surface municipal water (m3 per sec per day)"); //m_SWMuniWater
	this->m_dailyMetrics.SetLabel(5, "Allocated ground municipal water (m3 per sec per day)"); //m_GWMuniWater
	this->m_dailyMetrics.SetLabel(6, "Allocated surface irrigation water area (acres per day)"); //m_SWIrrAreaDy
	this->m_dailyMetrics.SetLabel(7, "Allocated ground irrigation water area (acres per day)"); //m_GWIrrAreaDy
	this->m_dailyMetrics.SetLabel(8, "Unexercised surface irrigation water right (m3 per sec per day)"); //m_SWUnExIrr
	this->m_dailyMetrics.SetLabel(9, "Unexercised surface municipal water right (m3 per sec per day)"); //m_SWUnExMun
	this->m_dailyMetrics.SetLabel(10, "Allocated surface irrigation water (m3 per day)");//m_SWIrrDuty
	this->m_dailyMetrics.SetLabel(11, "Allocated ground irrigation water (m3 per day)");//m_GWIrrDuty
	this->m_dailyMetrics.SetLabel(12, "Allocated surface irrigation water (mm per day)");//m_SWIrrWaterMmdy
	this->m_dailyMetrics.SetLabel(13, "Allocated ground irrigation water (mm per day)");//m_GWIrrWaterMmdy
   this->m_dailyMetrics.SetLabel(14, "m_fromOutsideBasinDaily (m3/sec)"); // m3/sec
	this->m_dailyMetrics.SetLabel(15, "Unsatisfied surface municipal demand (m3 per second per day)");// m_SWMunUnSatDemand
   this->m_dailyMetrics.SetLabel(16, "placeholder16"); // Unsatisfied ground irrigation request(m3 per second per day)");// m_GWIrrUnSatDemand
	this->m_dailyMetrics.SetLabel(17, "Unsatisfied ground municipal demand (m3 per second per day)");// m_GWMunUnSatDemand
   this->m_dailyMetrics.SetLabel(18, "Wasted Irrigation Water Rights (m3 per sec per day)");   // m_wastedWaterRateDy
   this->m_dailyMetrics.SetLabel(19, "Excess irrigation water (mm per day)");   // m_exceededWaterRateDy
	this->m_dailyMetrics.SetLabel(20, "Potentially Irrigated Irrigation Request (mm per day)");   // m_IrrWaterRequestDy
	m_pFlowModel->AddOutputVar("Daily AltWaterMaster Metrics", &m_dailyMetrics, "Daily AltWaterMaster Metrics");

   //AltWaterMaster Annual Metrics
   this->m_annualMetrics.SetName("ALTWM Annual Metrics");
   this->m_annualMetrics.SetSize(15, 0);
   this->m_annualMetrics.SetLabel(0, "Year");
   this->m_annualMetrics.SetLabel(1, "Potentially Irrigated Irrigation Request (acre-ft per year)"); //m_irrigableIrrWaterRequestYr
   this->m_annualMetrics.SetLabel(2, "Allocated irrigation Water (acre-ft per year)"); //m_irrigatedWaterYr
   this->m_annualMetrics.SetLabel(3, "Allocated surface irrigation water (acre-ft per year)"); //m_irrigatedSurfaceYr
   this->m_annualMetrics.SetLabel(4, "Allocated ground irrigation water (acre-ft per year)"); //m_irrigatedGroundYr
   this->m_annualMetrics.SetLabel(5, "m_GWnoWR_Yr_m3"); // "Unsatisfied surface irrigation request (acre-ft per year)"); //m_unSatisfiedIrrSurfaceYr
   this->m_annualMetrics.SetLabel(6, "placeholder6"); // "Unsatisfied ground irrigation request (acre-ft per year)"); //m_unSatisfiedIrrGroundYr
   this->m_annualMetrics.SetLabel(7, "Unsatisfied irrigation request (acre-ft per year)"); //m_unSatisfiedIrrigationYr
   this->m_annualMetrics.SetLabel(8, "Wasted Irrigation Water Rights (acre-ft per year)"); //m_wastedWaterVolYr
   this->m_annualMetrics.SetLabel(9, "Excess irrigation water (acre-ft per year)"); //m_exceededWaterVolYr
   this->m_annualMetrics.SetLabel(10, "Area where duty exceeds appropriated threshold (acres per year)"); //m_areaDutyExceeds
   this->m_annualMetrics.SetLabel(11, "Unsatisfied irrigation request duty exceeds  (acre-ft per year)"); //m_demandDutyExceeds
   this->m_annualMetrics.SetLabel(12, "Irrigation request non-appropriated days (acre-ft per year)"); //m_demandOutsideBegEndDates
   this->m_annualMetrics.SetLabel(13, "Inadequate flow length weighted reaches (meters per year)");//m_irrLenWtReachConflictYr
   this->m_annualMetrics.SetLabel(14, "Unsatisfied in-stream regulatory demand (m3 per second per year)");// m_unSatInstreamDemand
   m_pFlowModel->AddOutputVar("ALTWM Annual Metrics", &m_annualMetrics, "ALTWM Annual Metrics");

   // Annual Quick Check metrics
   this->m_QuickCheckMetrics.SetName("ALTWM Quick Check Metrics");
   this->m_QuickCheckMetrics.SetSize(7, 0);
   this->m_QuickCheckMetrics.SetLabel(0, "Year");
   this->m_QuickCheckMetrics.SetLabel(1, "Precip (mm H2O)"); // PRECIP_YR
   this->m_QuickCheckMetrics.SetLabel(2, "GW pumping (mm H2O)"); // IRGWAF_Y (ac-ft) + GWMUNALL_Y (m3)
   this->m_QuickCheckMetrics.SetLabel(3, "from outside the basin (mm H2O)"); // m_fromOutsideBasinDy (m3)
   this->m_QuickCheckMetrics.SetLabel(4, "AET (mm H2O)"); // ET_YR
   this->m_QuickCheckMetrics.SetLabel(5, "basin specific discharge (m3 H2O per m2 ground)");
   this->m_QuickCheckMetrics.SetLabel(6, "water balance residual fraction");
   m_pFlowModel->AddOutputVar("ALTWM Quick Check Metrics", &m_QuickCheckMetrics, "ALTWM Quick Check Metrics");

	if ( m_debug )
		{
	   //AltWaterMaster Annual Metrics Debug
		this->m_annualMetricsDebug.SetName("AltWaterMaster Annual Metrics Debug");
		this->m_annualMetricsDebug.SetSize(12, 0);
		this->m_annualMetricsDebug.SetLabel(0, "Year");	
		this->m_annualMetricsDebug.SetLabel(1, "Orchards vineyards and tree farms area > maxDuty (acres)"); //21 m_anGTmaxDutyArea21
		this->m_annualMetricsDebug.SetLabel(2, "Grass seed area > maxDuty (acres)"); //22 m_anGTmaxDutyArea22
		this->m_annualMetricsDebug.SetLabel(3, "Pasture area > maxDuty (acres)"); //23 m_anGTmaxDutyArea23
		this->m_annualMetricsDebug.SetLabel(4, "Wheat area > maxDuty (acres)"); //24 m_anGTmaxDutyArea24
		this->m_annualMetricsDebug.SetLabel(5, "Fallow area > maxDuty (acres)"); //25	m_anGTmaxDutyArea25
		this->m_annualMetricsDebug.SetLabel(6, "Corn area > maxDuty (acres)"); //26 m_anGTmaxDutyArea26
		this->m_annualMetricsDebug.SetLabel(7, "Clover area > maxDuty (acres)"); //27 m_anGTmaxDutyArea27
		this->m_annualMetricsDebug.SetLabel(8, "Hay area > maxDuty (acres)"); //28 m_anGTmaxDutyArea28
		this->m_annualMetricsDebug.SetLabel(9, "Other crops area > maxDuty (acres)"); //29 m_anGTmaxDutyArea29
		this->m_annualMetricsDebug.SetLabel(10, "A clover IDU where > maxDuty "); //23 m_pastureIDUGTmaxDuty
		this->m_annualMetricsDebug.SetLabel(11, "A clover IDU area where > maxDuty "); //23 m_pastureIDUGTmaxDutyArea
		m_pFlowModel->AddOutputVar("Annual AltWM Metrics Debug", &m_annualMetricsDebug, "Annaul AltWM Metrics Debug");

		//AltWM Daily Metrics Debug
		this->m_dailyMetricsDebug.SetName("AltWM Daily Metrics Debug"); 
		this->m_dailyMetricsDebug.SetSize(10, 0);
		this->m_dailyMetricsDebug.SetLabel(0, "Year");		
		this->m_dailyMetricsDebug.SetLabel(1, "Orchards vineyards and tree farms area > maxPOD (acres)"); //21 m_dyGTmaxPodArea21
		this->m_dailyMetricsDebug.SetLabel(2, "Grass seed area > maxPOD (acres)"); //22 m_dyGTmaxPodArea22
		this->m_dailyMetricsDebug.SetLabel(3, "Pasture area > maxPOD (acres)"); //23 m_dyGTmaxPodArea23
		this->m_dailyMetricsDebug.SetLabel(4, "Wheat area > maxPOD (acres)"); //24	m_dyGTmaxPodArea24
		this->m_dailyMetricsDebug.SetLabel(5, "Fallow area > maxPOD (acres)"); //25 m_dyGTmaxPodArea25
		this->m_dailyMetricsDebug.SetLabel(6, "Corn area > maxPOD (acres)"); //26	m_dyGTmaxPodArea26
		this->m_dailyMetricsDebug.SetLabel(7, "Clover area > maxPOD (acres)"); //27 m_dyGTmaxPodArea27
		this->m_dailyMetricsDebug.SetLabel(8, "Hay area > maxPOD (acres)"); //28 m_dyGTmaxPodArea28
		this->m_dailyMetricsDebug.SetLabel(9, "Other crops area > maxPOD (acres)"); //29	m_dyGTmaxPodArea29
		m_pFlowModel->AddOutputVar("Daily AltWM Metrics Debug", &m_dailyMetricsDebug, "Daily AltWM Metrics Debug");
		}

	char units = 'm';

	// if ( m_exportDistPodComid == 1 ) ExportDistPodComid(pFlowContext, units);

   int podArrayLen = (int)m_podArray.GetCount();
   CString msg;
   msg.Format("length of podArray = %d\n", podArrayLen);
   Report::Log(msg);
   
   // loop thru the IDUs: for each IDU with an irrigation water right, populate the IDU's STRM_ORDER attribute.
   for (MapLayer::Iterator idu = pLayer->Begin(); idu != pLayer->End(); idu++)
      {
      int wr_irrig_s = 0; pLayer->GetData(idu, m_colWR_IRRIG_S, wr_irrig_s);
      int wr_irrig_g = 0; pLayer->GetData(idu, m_colWR_IRRIG_G, wr_irrig_g);
      if (wr_irrig_s != 1 && wr_irrig_g != 1) continue;

      int idu_index = -1; pLayer->GetData(idu, m_colIDUIndex, idu_index);
      bool doneFlag = false;
      for (int podNdx = 0; !doneFlag && podNdx < podArrayLen; podNdx++)
         {
         WR_USE useCode = m_podArray[podNdx]->m_useCode;
         if (!IsIrrigation(useCode)) continue;

         int pouID = m_podArray[podNdx]->m_pouID;
         PouKeyClass pouLookupKey; pouLookupKey.pouID = pouID;
         vector<int> *pouIDUs = 0; pouIDUs = &m_pouInputMap[pouLookupKey];
         if (pouIDUs->size() <= 0) continue;

         for (int pouIDUs_ndx = 0; !doneFlag && pouIDUs_ndx < pouIDUs->size(); pouIDUs_ndx++)
            {
            int tempIDUndx = m_pouDb.GetAsInt(m_colPouIDU_INDEX, pouIDUs->at(pouIDUs_ndx));
            if (idu_index != tempIDUndx) continue;

            // The IDU is associated with this POD.

            // Populate the IDU's STRM_ORDER attribute.
            int reachcomid = m_podArray[podNdx]->m_reachComid;
            int reachNdx = pStreamLayer->FindIndex(m_colStrLayComid, m_podArray[podNdx]->m_reachComid, 0); 
            if (reachNdx < 0) continue; // This might happen if we're simulating a subbasin instead of the whole study area.
            Reach * pReach = pFlowContext->pFlowModel->FindReachFromIndex(reachNdx);
            pLayer->SetData(idu, m_colSTRM_ORDER, pReach->m_streamOrder);
            doneFlag = true;
            } // end of loop thru POUs associated with this POD
         } // end of loop thru PODs
      } // end of loop thru IDUs

   /* Create "WaterRights.csv" (the output file).
   PCTSTR WR_file = (PCTSTR)"WaterRights.csv";
   FILE *oFile = NULL;
   int errNo = fopen_s(&oFile, WR_file, "w");
   if (errNo != 0)
      {
      CString msg(" WW2100AP:ColdStart -  ERROR: Could not open output file ");
      msg += WR_file;
      Report::ErrorMsg(msg);
      return false;
      }
   fprintf(oFile, "IDU_INDEX, IDU_AREA_AC, POUID, OVERLAP_AREA_AC, POU_PERCENT, WATERRIGHTID, PODID, USECODE, YEAR, IN_USE, POD_STATUS, WREXISTS, PERMITCODE, POU_ROW\n");

   int podArrayLen = (int)m_podArray.GetCount();

   // loop thru the IDUs: for each IDU with WREXISTS!=0, find the active WaterRights objects associated with it.
   for (MapLayer::Iterator idu = pLayer->Begin(); idu != pLayer->End(); idu++)
      {
      CString msg;
      int count = 0;
      int wr_exists = 0;
      pLayer->GetData(idu, m_colWRExists, wr_exists);    //(idu, m_wrExistsColName, &wr_exists);
      if (wr_exists == 0) continue;

      int idu_index = -1; pLayer->GetData(idu, m_colIDUIndex, idu_index);
      float idu_area = 0.f; pLayer->GetData(idu, m_colIDUArea, idu_area);

      int pod_count = 0;
      int tot_pou_count_for_idu = 0;
      int waterrightid_changes = 0;
      int prev_waterrightid = -1;
      for (int podNdx = 0; podNdx < podArrayLen; podNdx++)
         {
         int pouID = m_podArray[podNdx]->m_pouID;
         PouKeyClass pouLookupKey; pouLookupKey.pouID = pouID;
         vector<int> *pouIDUs = 0; pouIDUs = &m_pouInputMap[pouLookupKey];
         if (pouIDUs->size() <= 0) continue;
         int pouIDUs_ndx, tempIDUndx;
         int pou_count = 0;
         for (pouIDUs_ndx = 0; pouIDUs_ndx < pouIDUs->size(); pouIDUs_ndx++)
            {
            tempIDUndx = m_pouDb.GetAsInt(m_colPouIDU_INDEX, pouIDUs->at(pouIDUs_ndx));
            if (idu_index != tempIDUndx) continue;
            pou_count++;
            int pouRow = pouIDUs->at(pouIDUs_ndx);
            int pouid = -1; pouid = m_pouDb.GetAsInt(m_colPUPouID, pouRow);
            float overlap_area = -1.f; overlap_area = m_pouDb.GetAsFloat(m_colPouArea, pouRow);
            float percent = -1.f; percent = m_pouDb.GetAsFloat(m_colPouPct, pouRow);
            int this_waterrightid = m_podArray[podNdx]->m_wrID;
            if (prev_waterrightid != -1 && this_waterrightid != prev_waterrightid) waterrightid_changes++;
            prev_waterrightid = this_waterrightid;
            int podid = m_podArray[podNdx]->m_podID;
			WR_USE useCode = m_podArray[podNdx]->m_useCode;
			WR_PERMIT permitCode = m_podArray[podNdx]->m_permitCode;
			int year = m_podArray[podNdx]->m_priorYr;
            bool in_use = m_podArray[podNdx]->m_inUse;
            WR_PODSTATUS pod_status = m_podArray[podNdx]->m_podStatus;

			float m2_per_ac = 4046.85642f;
			float idu_area_ac = idu_area / m2_per_ac;
			float overlap_area_ac = overlap_area / m2_per_ac;
			fprintf(oFile, "%d, %f, %d, %f, %f, %d, %d, %d, %d, %d, %d, %d, %d, %d\n",
               idu_index, idu_area_ac, pouid, overlap_area_ac, percent, this_waterrightid, podid, (int)useCode, year, in_use, (int)pod_status, wr_exists, permitCode, pouRow);
			if (overlap_area_ac > 1.01f*idu_area_ac)
				{
				CString msg;
				msg.Format(" AltWM::Init() overlap area > 101% of IDU area. IDU_INDEX = %d, POUID = %d, idu_area_ac = %f, overlap_area_ac = %f, pouRow = %d",
					idu_index, pouid, idu_area_ac, overlap_area_ac, pouRow);
				Report::LogWarning(msg);
				}

            } // end of loop thru pouIDUs
         if (pou_count < 1) continue;

         if (pou_count > 1)
            {
            msg.Format("idu_index %d for podNdx %d has %d POUs.", idu_index, podNdx, pou_count);
            Report::Log(msg);
            }
         pod_count++;
         tot_pou_count_for_idu += pou_count;
         } // end of loop on podNdx

      if (pod_count <= 0)
         {
         msg.Format("WW2100AP: idu %d had non-zero WREXISTS but no active water right data.  Clearing WREXISTS etc now.", idu_index);
         Report::Log(msg);
		 pLayer->SetData(idu_index, m_colWRExists, 0);
		 pLayer->SetData(idu, m_colWR_MUNI, 0); 
		 pLayer->SetData(idu, m_colWR_INSTRM, 0); 
		 pLayer->SetData(idu, m_colWR_IRRIG_S, 0); 
		 pLayer->SetData(idu, m_colWR_IRRIG_G, 0); 
		 }
      else if ((pod_count > 1 || tot_pou_count_for_idu>1) && waterrightid_changes>1)
         {
         msg.Format("WW2100AP: idu %d has %d PODs and a total of %d POUs with %d WATERRIGHTID changes\n",
            idu_index, pod_count, tot_pou_count_for_idu, waterrightid_changes);
         Report::Log(msg);
         }

      } // end of loop thru idus

   // close the output file
   fclose(oFile);
   */
   
   return true;
	}

bool AltWaterMaster::InitRun(FlowContext *pFlowContext)
	{

   MapLayer *pLayer = (MapLayer*)pFlowContext->pEnvContext->pMapLayer;

	MapLayer* pStreamLayer = (MapLayer*)pFlowContext->pFlowModel->m_pStreamLayer;

   if ( m_dynamicWRType == 1 ) DynamicWaterRights( pFlowContext, m_dynamicWRType, 0 );

	for (MapLayer::Iterator idu = pLayer->Begin(); idu != pLayer->End(); idu++)
		{
		//Initiate arrays for water right conflict metrics and allocation arrays
		m_iduSWLevelOneIrrWRSOAreaArray[idu] = 0.0f;
		m_iduSWLevelOneMunWRSOAreaArray[idu] = 0.0f;
		m_pouSWLevelOneIrrWRSOAreaArray[idu] = 0.0f;
		m_pouSWLevelOneMunWRSOAreaArray[idu] = 0.0f;
		m_iduSWLevelTwoIrrWRSOAreaArray[idu] = 0.0f;
		m_iduSWLevelTwoMunWRSOAreaArray[idu] = 0.0f;
		m_pouSWLevelTwoIrrWRSOAreaArray[idu] = 0.0f;
		m_pouSWLevelTwoMunWRSOAreaArray[idu] = 0.0f;
		m_iduSWIrrWRSOIndex[idu] = -1;
		m_iduSWMunWRSOIndex[idu] = -1;
		m_iduSWIrrWRSOWeek[idu] = -1;
		m_iduSWMunWRSOWeek[idu] = -1;
		m_iduLocalIrrRequestArray[idu] = 0.0f;
		m_iduSWIrrWRSOLastWeek[idu] = -999;
	   m_iduSWMunWRSOLastWeek[idu] = -999;
		m_iduAnnualIrrigationDutyArray[idu] = 0.f;
		m_iduIsIrrOutSeasonDy[idu] = 0;
		m_iduActualIrrRequestDy[idu] = 0.0f;

		// surface water irrigation 
		m_iduSWUnAllocatedArray[idu] = 0.0f;
		m_iduSWIrrArrayDy[idu] = 0.0f;
		m_iduSWIrrArrayYr[idu] = 0.0f;
		m_iduSWMuniArrayDy[idu] = 0.0f;
		m_iduSWMuniArrayYr[idu] = 0.0f;
		m_iduSWUnsatMunDmdYr[idu] = 0.0f;
		m_iduSWAppUnSatDemandArray[idu] = 0.0f;
		m_iduSWUnExerIrrArrayDy[idu] = 0.0f;
		m_iduSWUnExerMunArrayDy[idu] = 0.0f;
		m_iduSWUnSatIrrArrayDy[idu] = 0.0f;
		m_iduSWUnSatMunArrayDy[idu] = 0.0f;
	   m_iduGWUnSatMunArrayDy[idu] = 0.0f;

		// ground water irrigation 
		m_iduGWUnAllocatedArray[idu] = 0.0f;
		m_iduGWIrrArrayDy[idu] = 0.0f;
		m_iduGWIrrArrayYr[idu] = 0.0f;
		m_iduGWMuniArrayDy[idu] = 0.0f;
		m_iduGWMuniArrayYr[idu] = 0.0f;
		m_iduGWUnsatIrrReqYr[idu] = 0.0f;
		m_iduGWUnsatMunDmdYr[idu] = 0.0f;
		m_iduGWAppUnSatDemandArray[idu] = 0.0f;
		m_iduGWUnExerIrrArrayDy[idu] = 0.0f;
		m_iduGWUnExerMunArrayDy[idu] = 0.0f;
		m_iduIrrWaterRequestYr[idu] = 0.0f;

		//total irrigation 
		m_maxTotDailyIrr[idu] = 0.0f;
		m_aveTotDailyIrr[idu] = 0.0f;
		m_nIrrPODsPerIDUYr[idu] = 0;
		m_iduWastedIrr_Yr[idu] = 0.0f;
		m_iduExceededIrr_Yr[idu] = 0.0f;
		m_iduWastedIrr_Dy[idu] = 0.0f;
		m_iduExceededIrr_Dy[idu] =0.0f;

		//Duty
		m_iduExceedDutyLog[idu] = 0;

		}
	
   bool readOnlyFlag = pLayer->m_readOnly;
   pLayer->m_readOnly = false;
   pLayer->SetColData(m_colGAL_CAP_DY, VData(0), true);
   pLayer->m_readOnly = readOnlyFlag;

	int reachCount = pFlowContext->pFlowModel->GetReachCount();

	for (int i = 0; i < reachCount; i++)
		{

		float reachLength = 0.0f;
			
		m_reachDaysInConflict[i] = 0;

		m_dailyConflictCnt[i] = 0;

		pStreamLayer->GetData( i, m_colReachLength, reachLength);
									
		m_basinReachLength += reachLength;

		} // endfor reach

	return true;
   } // end of AltWaterMaster::InitRun()


bool AltWaterMaster::Step(FlowContext *pFlowContext)
	{
	// basic idea.  
	// 1. The Point of Diversion (POD) input data file should be sorted asending by 
	//    priority year, priority day of year (doy), season begin doy, and season end doy
	// 2. Iterate through POD water rights input data file
	// 3. For each water right (WR), allocate the right if possible, based on demand, and 
	//    the Point of Use data input file.

	int   wrUseCode;     // water right use code
	int   wrBeginDoy;    // water right begin day of year
	int   wrEndDoy;      // water right end day of year
	int   wrPermitCode;
	int   plantDate = 0;
	int   harvDate = 0;
	vector<int> *iduNdxVec = 0;

	MapLayer *pLayer = (MapLayer*)pFlowContext->pEnvContext->pMapLayer;
   MapLayer* pStreamLayer = (MapLayer*)pFlowContext->pFlowModel->m_pStreamLayer;

	// 0 based year of run
	m_envTimeStep = pFlowContext->pEnvContext->yearOfRun;

   {
   CString msg;
   int doy = pFlowContext->dayOfYear;
   msg.Format("*** AltWM::Step() year = %d, day = %d", pFlowContext->pEnvContext->yearOfRun, doy);
   if (((float)doy/10.f)*10.f==doy) 
      Report::Log(msg);
   }

	// set to zero at start of processing for each daily time step
   for (int uga = 0; uga <= MAX_UGA_NDX; uga++) 
      {
      m_ugaUWallocatedDay[uga] = 0.f;
      m_ugaLocalUWdemandArray[uga] = 0.f;
      } // end of loop on UGAs
   m_SWIrrAreaDy = 0.0f;
	m_GWIrrAreaDy = 0.0f;
	m_SWUnExIrr = 0.0f;
	m_SWUnExMun = 0.0f;
	m_regulatoryDemand = 0.0f;
	m_SWIrrDuty = 0.0f;
	m_GWIrrDuty = 0.0f;
	m_SWIrrWater= 0.0f;
	m_SWIrrWaterMmDy = 0.0f;
	m_GWIrrWater= 0.0f;
	m_GWIrrWaterMmDy = 0.0f;
	m_SWMuniWater=0.0f;
	m_GWMuniWater=0.0f;
	m_wastedWaterRateDy = 0.0f;	
   m_exceededWaterRateDy = 0.0f;
	m_IrrFromAllocationArrayDy = 0.0f;
	m_IrrFromAllocationArrayDyBeginStep = 0.0f;
	m_SWIrrWater = 0.0f;
	m_SWIrrWaterMmDy = 0.0f;
   m_SWMuniWater = 0.0f;
	m_SWIrrUnSatDemand = 0.0f;
	m_SWMunUnSatDemand = 0.0f;	
	m_IrrWaterRequestDy = 0.0f;	
	m_GWIrrWater = 0.0f;
	m_GWIrrWaterMmDy = 0.0f;
	m_GWMuniWater = 0.0f;
	m_GWIrrUnSatDemand = 0.0f;
	m_GWMunUnSatDemand = 0.0f;	
	m_SWunallocatedIrrWater = 0.0f;
	m_SWunsatIrrWaterDemand = 0.0f;  
   m_fromOutsideBasinDy = 0.f;

	int reachCount = pFlowContext->pFlowModel->GetReachCount();

	#pragma omp parallel for
	for (int i = 0; i < reachCount; i++)
		{
		Reach *pReach = pFlowContext->pFlowModel->GetReach(i); // Note: these are guaranteed to be non-phantom
		pReach->m_availableDischarge = pReach->GetDischarge() * m_fractionDischargeAvail; //only X% of flow available, set in .xml file
		pReach->m_instreamWaterRightUse = 0.;
		pFlowContext->pFlowModel->m_pStreamLayer->m_readOnly = false;
		pFlowContext->pFlowModel->m_pStreamLayer->SetData(i, m_colWRConflictDy, 0);
		pFlowContext->pFlowModel->m_pStreamLayer->m_readOnly = true;
		m_dailyConflictCnt[i] = 0;
		}

	for (MapLayer::Iterator idu = pLayer->Begin(); idu != pLayer->End(); idu++)
		{
		m_iduSWIrrArrayDy[idu] = 0.f;
		m_iduGWIrrArrayDy[idu] = 0.f;
		m_iduSWMuniArrayDy[idu] = 0.f;
		m_iduGWMuniArrayDy[idu] = 0.f;
		m_nIrrPODsPerIDUYr[idu] = 0;
		m_iduActualIrrRequestDy[idu] = 0.0f;
		m_iduSWUnExerIrrArrayDy[idu] = 0.f;
		m_iduGWUnExerIrrArrayDy[idu] = 0.0f;
		m_iduSWUnExerMunArrayDy[idu] = 0.f;
		m_iduGWUnExerMunArrayDy[idu] = 0.0f;
		m_iduSWUnSatIrrArrayDy[idu] = 0.0f;
		m_iduSWUnSatMunArrayDy[idu] = 0.0f;
		//m_iduGWUnSatIrrArrayDy[idu] = 0.0f;
	   m_iduGWUnSatMunArrayDy[idu] = 0.0f;
		m_iduWastedIrr_Dy[idu] = 0.0f;
		m_iduExceededIrr_Dy[idu] =0.0f;

		m_IrrFromAllocationArrayDyBeginStep += m_pWaterAllocation->m_iduIrrAllocationArray[idu];

		int lulcA = 0;
		float idu_area = 0.0f;

		pLayer->GetData( idu, m_colLulc_A, lulcA );
		
      float biological_water_demand = m_pWaterAllocation->m_iduIrrRequestArray[idu];
		pLayer->GetData(idu, m_colIrrigate, m_irrigateDecision);
      float smoothed_irr_req = 0.f;
      int doy0 = pFlowContext->dayOfYear; // day of year, Jan 1 = 0
      if (m_irrigateDecision == 1 && 
         (doy0 + 1) >= m_irrDefaultBeginDoy && (doy0 + 1) <= m_irrDefaultEndDoy && 
         biological_water_demand > 0.f)
         {
         float unsmoothed_irr_req = biological_water_demand < m_maxIrrDiversion ? 
            biological_water_demand : m_maxIrrDiversion;

         // Update smoothed_irr_req.
         if ((doy0 + 1) == m_irrDefaultBeginDoy) smoothed_irr_req = unsmoothed_irr_req;
         else if ((doy0 + 1) > m_irrDefaultBeginDoy && (doy0 + 1) <= m_irrDefaultEndDoy)
            {
            float tau = 7.f; // smoothing time constant
            float tau_eff = ((doy0 + 1) - m_irrDefaultBeginDoy) < tau ? 
               ((doy0 + 1) - m_irrDefaultBeginDoy) : tau;

            float prev_smoothed_irr_req = 0.f;
            pLayer->GetData(idu, m_colActualIrrRequestDy, prev_smoothed_irr_req); // mm/day; read yesterday's IRRACRQ_D

            smoothed_irr_req = prev_smoothed_irr_req*exp(-1.f / tau_eff) + unsmoothed_irr_req*(1.f - exp(-1.f / tau_eff));

            }
         else smoothed_irr_req = 0.f;
         }
      else smoothed_irr_req = 0.f;

      m_iduLocalIrrRequestArray[idu] = smoothed_irr_req;

	   // get from IDU layer if this IDU has a water right
      int wrExists = 0;  pLayer->GetData(idu, m_colWREXISTS, wrExists);
		pLayer->GetData( idu, m_colPlantDate, plantDate );
      pLayer->GetData( idu, m_colHarvDate,  harvDate );
		pLayer->GetData(idu, m_colAREA, idu_area);

	   // get the use code from the WREXISTS code
	   unsigned __int16 iduUse = GetUse( wrExists );

		// get the use code from the WREXISTS code
	   unsigned __int8 iduPermit = GetPermit( wrExists );
						
		//Reset arrays for next envision time step
		if (pFlowContext->dayOfYear == 0)
			{
			// conflict
			m_iduSWIrrWRSOWeek[idu] = -1;
			m_iduSWMunWRSOWeek[idu] = -1;
			m_iduSWLevelOneIrrWRSOAreaArray[idu] = 0.0f;
			m_iduSWLevelOneMunWRSOAreaArray[idu] = 0.0f;
			m_pouSWLevelOneIrrWRSOAreaArray[idu] = 0.0f;
			m_pouSWLevelOneMunWRSOAreaArray[idu] = 0.0f;
			m_iduSWLevelTwoIrrWRSOAreaArray[idu] = 0.0f;
			m_iduSWLevelTwoMunWRSOAreaArray[idu] = 0.0f;
			m_pouSWLevelTwoIrrWRSOAreaArray[idu] = 0.0f;
			m_pouSWLevelTwoMunWRSOAreaArray[idu] = 0.0f;
			m_iduAnnualIrrigationDutyArray[idu] = 0.0f;

			//surface water
			m_iduSWUnAllocatedArray[idu] = 0.0f;
			m_iduSWUnsatMunDmdYr[idu] = 0.0f;

			m_iduSWAppUnSatDemandArray[idu] = 0.0f;			
			m_iduSWIrrArrayYr[idu] = 0.0f;
			m_iduSWMuniArrayYr[idu] = 0.0f;

		   //ground water
			m_iduGWUnAllocatedArray[idu] = 0.0f;
			m_iduGWUnsatIrrReqYr[idu] = 0.0f;
			m_iduGWUnsatMunDmdYr[idu] = 0.0f;

			m_iduGWAppUnSatDemandArray[idu] = 0.0f;
			m_iduGWIrrArrayYr[idu] = 0.0f;
			m_iduGWMuniArrayYr[idu] = 0.0f;
			
			//totals
			m_iduIrrWaterRequestYr[idu] = 0.0f;
			m_maxTotDailyIrr[idu] = 0.0f;
			m_aveTotDailyIrr[idu] = 0.0f;

			//duty
			m_iduExceedDutyLog[idu] = 0;
			}

		// m_iduIrrRequestArray is in units mm/day, conversion reference http://www.fao.org/docrep/x0490e/x0490e04.htm
		// ET mm/day = 10 m3/ha/day
		// AREA in IDU layer is assumed to be meters squared m2
		// availSourceFlow = m3/sec
		// 10000 m2/ha

		float iduAreaM2 = 0.0f;

		pLayer->GetData(idu, m_colAREA, iduAreaM2);

		float iduAreaAc = iduAreaM2 * ACRE_PER_M2;

      float iduIrrRequest_acft = (m_iduLocalIrrRequestArray[idu] / 1000.f) * iduAreaM2 / M3_PER_ACREFT; // acre-feet per day

		if (m_irrigateDecision == 1)
			{
         m_IrrWaterRequestDy += m_iduLocalIrrRequestArray[idu];
			m_irrigableIrrWaterRequestYr += iduIrrRequest_acft; //irrigable acre-feet per day
			}

		m_irrWaterRequestYr += iduIrrRequest_acft;  // acre-feet per day
		m_iduIrrWaterRequestYr[idu] += iduIrrRequest_acft; // acre-feet per day
				
      float dailyUWdemand_m3_per_sec;
      pLayer->GetData(idu, m_colUDMAND_DY, dailyUWdemand_m3_per_sec); // UDMAND_DY is calculated and stored by DailyUrbanWaterDemand::CalcDailyUrbanWaterDemand().
      int uga = 0; pLayer->GetData(idu, m_colUGB, uga);
      if (uga > 0 && uga <= MAX_UGA_NDX) m_ugaLocalUWdemandArray[uga] += dailyUWdemand_m3_per_sec; // m3/sec

		}//end IDU

      if (pFlowContext->dayOfYear == 0)
         {
         for (int uga = 0; uga <= MAX_UGA_NDX; uga++)
            {
            CString msg;
            msg.Format("*** AltWaterMaster::Step() uga = %d, m_ugaLocalUWdemandArray[uga] = %f", uga, m_ugaLocalUWdemandArray[uga]);
            Report::Log(msg);
            } // end of loop on UGAs
         } // end of if day of year == 0 

	// Begin looping through POD input data file
	for (int i = 0; i < (int)m_podArray.GetSize(); i++)
		{
		// reset instream for each water right
		m_inStreamUseCnt = 0;  
		
		vector<int> *pouIDUs = 0;

		// Get current WR
		WaterRight *pRight = m_podArray[i];
      pRight->m_stepRequest = 0.f;

		// if the distance from a POD to a reach is greater than or equal to user defined max distance to reach, specified in in .xml, skip POD
		if ( pRight->m_distanceToReach >= m_maxDistanceToReach ) continue;

		// if this exist in POD input file.  If also exist in IDU layer, IDU layer value is used.
		// If neither exist in IDU layer or POD file, then the default duty set in .xml is used.
		if ( m_colMaxDutyFile != -1 ) 
			m_maxDuty = pRight->m_maxDutyPOD;
		
		// initiate variables at first of year
		if (pFlowContext->dayOfYear == 0)
			{
			pRight->m_nDaysPerYearSO = 0;
			}

		
      if (pRight->m_suspendForYear) continue; // Skip if the water right has been shut off by regulatory action.

		// Get POUID (aka Place of Use) for current Permit
		int pouID = pRight->m_pouID;
      if (pRight->m_useCode == WRU_MUNICIPAL && pouID < 0)
         { // Interpret the pouID as -UGB
         int uga = -pouID;
         if (uga > MAX_UGA_NDX) break;

         if (pFlowContext->dayOfYear == 0)
            {
            CString msg;
            msg.Format("*** AltWaterMaster::Step() WR PODindex = %d is associated with UGA %d ", i, uga);
            Report::Log(msg);
            }

         // Satisfy as much of the UGA's remaining municipal water demand as possible from this WR.
         float maxPODrate = pRight->m_podRate / (FT3_PER_M3); // Convert PODRATE in POD file from cfs to maxPODrate in m3/s
         float request = min(m_ugaLocalUWdemandArray[uga], maxPODrate); // m3/sec H2O
         if (request <= 0.f) continue;
         float allocated_water = 0.f; // m3 H2O
         switch (pRight->m_permitCode)
            {
            case WRP_SURFACE:
               AllocateSWtoUGA(pRight, pStreamLayer, uga, request, &allocated_water);
               break;
            case WRP_GROUNDWATER:
               AllocateGWtoUGA(pRight, uga, request, &allocated_water);
               break;
            default: break;
            } // end of switch on permitCode

         if (pFlowContext->dayOfYear == 0)
            {
            CString msg;
            msg.Format("*** AltWaterMaster::Step() uga = %d, request = %f, allocated_water = %f ", uga, request, allocated_water);
            Report::Log(msg);
            }

         m_ugaUWallocatedDay[uga] += allocated_water;
         m_ugaLocalUWdemandArray[uga] -= allocated_water;

         continue; // We're done with this point of diversion for this water right.
         }

      // Interpret the pouID as a collection of IDUs

		// Build lookup key into point of use (POU) map/lookup table
		m_pouLookupKey.pouID = pouID;

		// Returns vector of indexs into the POU input data file. Used for relating to current water right POU to polygons in IDU layer
		pouIDUs = &m_pouInputMap[m_pouLookupKey];

		// if a PODID does not have a POUID, consider next WR
		if (pouIDUs->size() == 0) continue;
			
		float dailyDemand = 0.f;
		float POUarea = 0.f;

		m_idusPerPou = (int) pouIDUs->size();

		int irrPousPerPod = 0; // pous greater with over lap greater than user specified threshold
		int countConflict = 0;

		// Diversion is a go. Begin looping through IDUs Associated with WRID and PLace of Use (POU)
		for (int j = 0; j < pouIDUs->size(); j++)
			{			
			//this is temporary until new instream water right method is developed
			m_inStreamUseCnt++;

			int tempIDUNdx = m_pouDb.GetAsInt(m_colPouIDU_INDEX, pouIDUs->at(j));

			//build key into idu index map
			m_iduIndexLookupKey.iduIndex = tempIDUNdx;

			//returns vector with the idu index for idu layer
			iduNdxVec = &m_IDUIndexMap[m_iduIndexLookupKey];

			//if no index is return, then idu the POU is associated with is not in current layer
			if (iduNdxVec->size() == 0) continue;

			int iduNdx = iduNdxVec->at(0);
         int wrExists = 0; pLayer->GetData(iduNdx, m_colWREXISTS, wrExists);
         if (wrExists == 0) continue;

         int uga = -1; pLayer->GetData(iduNdx, m_colUGB, uga);

			// if this exist in IDU layer.  If also exist in IDU layer, IDU layer value is used.
		   // If neither exist in IDU layer or POD file, then the default duty set in .xml is used.
			if (m_colMaxDutyLayer != -1) pLayer->GetData(iduNdx, m_colMaxDutyLayer, m_maxDuty);
			
			pLayer->GetData(iduNdx, m_colIrrigate, m_irrigateDecision);

			// Get the areal percent of the POU for current WRid and IDU_INDEX
			float pctPou = m_pouDb.GetAsFloat(m_colPouPct, pouIDUs->at(j));

			pctPou = pctPou/100;

			// Get the area of the POU for current WRid and IDU_INDEX
			float areaPou = m_pouDb.GetAsFloat(m_colPouArea, pouIDUs->at(j));

			float iduAreaM2 = 0.0f;

			pLayer->GetData( iduNdx, m_colAREA, iduAreaM2 );
		
			wrUseCode = -99;	// water right use code
			wrBeginDoy = -99; // water right begin day of year
			wrEndDoy = -99;	// water right end day of year
			wrPermitCode = 0;
			wrExists = 0;		// from the IDU layer, bitwise to determine WR_USE and WR_PERMIT

			// In the Water Rights data base, irrigation seasonality may not be specified (old water rights). 
			// In the case of Oregon water rights, if seasonality for a water 
			// right is unknown, then the begin and end irrigation DOYs in the data base are 1 and 365 respectively.  
			// However, Oregon law does impose a default seasonality, this default seasonality should be set in flow's .xml

			// get the use code from the WREXISTS code
			unsigned __int16 iduUse = GetUse( wrExists );
				
			if ( IsIrrigation(iduUse) && pRight->m_useCode == WRU_IRRIGATION )
				{					
				irrPousPerPod += 1; // count pous that have overlap greater than 60%
						
				if ( pRight->m_beginDoy == 1 && pRight->m_endDoy == 365 )
					{
					if ( m_irrDefaultBeginDoy > 0 ) // -1 is a default value set in constructor (1 base)
						{
						pRight->m_beginDoy = m_irrDefaultBeginDoy;
						pRight->m_endDoy   = m_irrDefaultEndDoy;
						}
					else
						{
						CString msg;
		            msg.Format("AltWM::Run: Irrigation default begin Day of Year is out of bounds and should be specified in flow's .xml file %d", iduNdx);
		            Report::Log(msg);
						}
					}				
				} // end of if ( IsIrrigation(iduUse) && pRight->m_useCode == WRU_IRRIGATION )

			if ( m_irrigateDecision == 1 )
				{
				m_wrIrrigateFlag = true;
				}
			else if ( m_irrigateDecision == 0 )
				{
				m_wrIrrigateFlag = false;
				countConflict += 1;  // part of use it or lose it
				}

			// Simplify some names for conditional statement       
			wrUseCode = pRight->m_useCode;
			wrBeginDoy = pRight->m_beginDoy;
			wrEndDoy = pRight->m_endDoy;
			wrPermitCode = pRight->m_permitCode;

			// Use code from layer, use code from WR, and seasonality of WR (doy is zero based, water right days are 1 based)
			if (( pFlowContext->dayOfYear + 1 >= wrBeginDoy ) &&
				(  pFlowContext->dayOfYear + 1 <= wrEndDoy ))
				{
				switch (wrPermitCode)
					{

					case WRP_SURFACE:
                  if (wrUseCode == WRU_MUNICIPAL && 1 <= uga && uga <= MAX_UGA_NDX) break; // muni water in UGAs handled elsewhere
                  if (wrUseCode != WRU_IRRIGATION || (m_wrIrrigateFlag && m_iduLocalIrrRequestArray[iduNdx] > 0.f)) 
                     AllocateSurfaceWR(pFlowContext, pRight, iduNdx, pctPou, areaPou);
						break;

					case WRP_GROUNDWATER:
                  if (wrUseCode == WRU_MUNICIPAL && 1 <= uga && uga <= MAX_UGA_NDX) break; // muni water in UGAs handled elsewhere
                  AllocateWellWR(pFlowContext, pRight, iduNdx, pctPou, areaPou);
						break;

					default: // ignore everything else
						break;
					}
				} // end water right in season
			else
				{

				if ( m_colWREXISTS != -1 )
					{
					
					// get from IDU layer if this IDU has a water right
					pLayer->GetData( iduNdx, m_colWREXISTS, wrExists );
					pLayer->GetData( iduNdx, m_colPlantDate, plantDate );
					pLayer->GetData( iduNdx, m_colHarvDate,  harvDate );	
														
					float irrRequest = m_iduLocalIrrRequestArray[iduNdx];//mm/d

					float iduDemandAcreFt = irrRequest / 1000.0f * iduAreaM2 / M3_PER_ACREFT; // acre-feet 

					// get the use code from the WREXISTS code
					unsigned __int16 iduUse = GetUse( wrExists );
					
					int beenSeen = m_iduIsIrrOutSeasonDy[iduNdx]; // don't think I need this, keep for now

					int doy = pFlowContext->dayOfYear;

					if ( IsIrrigation(iduUse) &&  m_irrigateDecision == 1 &&  pRight->m_useCode == WRU_IRRIGATION ) 
						m_demandOutsideBegEndDates += iduDemandAcreFt;

					m_iduIsIrrOutSeasonDy[iduNdx] = 1;
				   } //end if WREXIST
				} //end if water right out of season
			} // end for idus per pou

		// if none of the IDUs within a POU, associated with the POD, attempt to irrigate. This indicates water right holder
		// deciding to not exercise all of the water right.  Part of use it, or lose it
		if ( irrPousPerPod == countConflict ) pRight->m_inConflict = true;

		} // end for POD prior appropriation list

   // Tally up unsatisfied irrigation demand and distribute UGA urban water use to the IDUs
   for (MapLayer::Iterator idu = pLayer->Begin(); idu != pLayer->End(); idu++)
      {
      float totMuniAllocatedDy = m_iduSWMuniArrayDy[idu] + m_iduGWMuniArrayDy[idu]; // m3/sec
      float totSWMuniAllocatedDy = m_iduSWMuniArrayDy[idu];
      float totGWMuniAllocatedDy = m_iduGWMuniArrayDy[idu];

      pLayer->m_readOnly = false;
      pLayer->SetData(idu, m_colMUNIALLO_D, totMuniAllocatedDy);
      pLayer->m_readOnly = true;

      float iduPop = 0; pLayer->GetData(idu, m_colPOP, iduPop);
      int wr_muni = 0; pLayer->GetData(idu, m_colWR_MUNI, wr_muni);
      int uga = 0; pLayer->GetData(idu, m_colUGB, uga);
      float iduUWdemand = 0.f; pLayer->GetData(idu, m_colUDMAND_DY, iduUWdemand); // m3/sec
      if (iduPop > 0.f && (uga < 1 || uga > MAX_UGA_NDX) && wr_muni == 0 && iduUWdemand > 0.f)
         { // This IDU is outside all the UGAs, has people, doesn't have a municipal water right, but does have positive urban water demand, so assume it has a well.
         m_iduGWMuniArrayDy[idu] += iduUWdemand; // m3/sec 
         m_iduGWMuniArrayYr[idu] += iduUWdemand * SEC_PER_DAY; // m3 H2O
         m_GWMuniWater += iduUWdemand; // m3/sec 
         m_GWnoWR_Yr_m3 += iduUWdemand * SEC_PER_DAY; // m3 H2O
         m_pWaterAllocation->m_iduNonIrrAllocationArray[idu] += iduUWdemand; // m3/s
         m_pWaterAllocation->m_iduYrMuniArray[idu] += iduUWdemand; //m3/sec;
         }

      float unsatisfied_request_mm = 0.f;

      bool readOnlyFlag = pLayer->m_readOnly;
      pLayer->m_readOnly = false;

      int irr_decision = 0; pLayer->GetData(idu, m_colIrrigate, irr_decision);
      if (irr_decision == 1) 
         { // Irrigation is desired today.
         int wr_shutoff = 0; pLayer->GetData(idu, m_colWRShutOff, wr_shutoff);
         unsatisfied_request_mm = m_iduLocalIrrRequestArray[idu];
         // Don't count it as a shortage if the IDU's water right has been shut off by regulatory action.
         bool shortageFlag = wr_shutoff == 0 && unsatisfied_request_mm > 0.01f;
         if (shortageFlag)
            {
            float idu_area_m2 = 0.f; pLayer->GetData(idu, m_colAREA, idu_area_m2);
            float unsatisfied_request_m3 = (unsatisfied_request_mm / 1000.f) * idu_area_m2;
            float unsatisfied_request_acft = unsatisfied_request_m3 / M3_PER_ACREFT;
            m_iduUnsatIrrReqst_Yr[idu] += unsatisfied_request_acft;
            m_unSatisfiedIrrigationYr += unsatisfied_request_acft;

            // Decrement IDUH2O_EST by the amount of the request which went unfulfilled.
            float avail_water_est_orig = 0.f; pLayer->GetData(idu, m_colIDUH2O_EST, avail_water_est_orig); // mm
            float avail_water_est_new = avail_water_est_orig - unsatisfied_request_mm / (1.f + 0.25f /* m_irrigLossFactor */);
            if (avail_water_est_new < 0.f) avail_water_est_new = 0.f;
/*               if (idu == 46864)
               {
               CString msg;
               msg.Format("AllocateSurfaceWR for IDU 46864: doy = %d, avail_water_est_orig = %f, unsatisfied_request_mm = %f, avail_water_est_new = %f",
                  pFlowContext->dayOfYear, avail_water_est_orig, unsatisfied_request_mm, avail_water_est_new);
               Report::Log(msg);
               } */
            pLayer->SetData(idu, m_colIDUH2O_EST, avail_water_est_new);
            pLayer->SetData(idu, m_colIRR_STATE, IRR_PARTIAL);

            } // end of if (shortageFlag)

         LogWeeklySurfaceWaterIrrigationShortagesByIDU( idu, shortageFlag);

         } // end of block for IDUs for which irrigation was desired

      pLayer->SetData(idu, m_colUnSatIrrRequest, unsatisfied_request_mm);

      pLayer->m_readOnly = readOnlyFlag;
      } // end of loop through IDUs to tally up unsatisfied irrigation demand

    for (int uga = 1; uga <= MAX_UGA_NDX; uga++) m_ugaUWshortageYr[uga] += m_ugaLocalUWdemandArray[uga] * SEC_PER_DAY;

	//all water rights have been exercised for relevant IDUs, now aggregate use per IDU to HRU Layer
	AggregateIDU2HRU(pFlowContext);

   // Now dispose of the urban water: some goes to ET, some goes to the ground, much goes back to the UGA's discharge point.
   FateOfUGA_UrbanWater(pFlowContext);

	return true;
	} // end of Step()


bool AltWaterMaster::AllocateSWtoUGA(WaterRight *pRight, MapLayer *pStreamLayer, int uga, float request, float *pAllocated_water) // quantities in m3/sec H2O
   {
   *pAllocated_water = 0.f;
   Reach *pReach = pRight->m_pReach;
   if (pRight->m_reachComid == -99)
      { // Source is outside the basin, e.g. Bull Run reservoir.
      *pAllocated_water = request;
      m_fromOutsideBasinDy += request;
      }
   else if (pReach != NULL)
      {
      float availSourceFlow;
      if (m_colMinFlow >= 0) pStreamLayer->GetData(pReach->m_polyIndex, m_colMinFlow, m_minFlow);
      else m_minFlow = 0.f;

      availSourceFlow = (pReach->m_availableDischarge * m_fractionDischargeAvail) - pReach->m_instreamWaterRightUse - m_minFlow; // m3/sec
      if (availSourceFlow <= 0.0) return(false);

      *pAllocated_water = min(request, availSourceFlow);
      pReach->AddFluxFromGlobalHandler(-(*pAllocated_water * SEC_PER_DAY)); //m3/d
      pReach->m_availableDischarge -= *pAllocated_water;
      }
   else return(false);

   m_SWMuniWater += *pAllocated_water; // m3/sec 
   m_ugaUWfromSW[uga] += *pAllocated_water * SEC_PER_DAY; // m3 H2O
   return(true);
   } // end of AllocateSWtoUGA()


   bool AltWaterMaster::AllocateGWtoUGA(WaterRight *pRight, int uga, float request, float *pAllocated_water) // quantities in m3/sec H2O
      {
      *pAllocated_water = request;

      m_GWMuniWater += *pAllocated_water; // m3/sec 
      m_ugaUWfromGW[uga] += *pAllocated_water * SEC_PER_DAY; // m3 H2O
      return(true);
      } // end of AllocateGWtoUGA()


   void AltWaterMaster::FateOfUGA_UrbanWater(FlowContext * pFlowContext)
      { // Dispose of the urban water: some goes to ET, some goes to the ground, much goes back to the UGA's discharge point.
      MapLayer* pStreamLayer = pFlowContext->pFlowModel->m_pStreamLayer;
      // int PortlandDischargeComID = 23815042; // "Willamette at Portland" 

      for (int uga = 1; uga <= MAX_UGA_NDX; uga++) if (m_ugaUWallocatedDay[uga] > 0.f)
         {
         m_ugaUWallocatedYr[uga] += m_ugaUWallocatedDay[uga] * SEC_PER_DAY;

         int dischargeComID = m_ugaDischargeComID[uga];
         if (dischargeComID < 0) continue; 
         int reachNdx = pStreamLayer->FindIndex(m_colStrLayComid, dischargeComID, 0);
         if (reachNdx < 0) continue; // This might happen if we're simulating a subbasin instead of the whole study area.
         Reach *pReach = pFlowContext->pFlowModel->FindReachFromIndex(reachNdx);
         pReach->AddFluxFromGlobalHandler(m_ugaUWallocatedDay[uga] * SEC_PER_DAY); // It all goes back to the stream right now.
         }
      } // end of FateOfUGA_UrbanWater()



   bool AltWaterMaster::AllocateSurfaceWR(FlowContext *pFlowContext, WaterRight *pRight, int iduNdx, float pctPou, float areaPou)
	{
	MapLayer *pLayer = (MapLayer*)pFlowContext->pEnvContext->pMapLayer;

	MapLayer* pStreamLayer = (MapLayer*)pFlowContext->pFlowModel->m_pStreamLayer;

	// each water right is associated with a reach in the stream layer. This relationship is stored in the POD lookup table and assigned
	// to pRight when loading the lookup table.
	Reach *pReach = pRight->m_pReach;
	int reachNdx = pStreamLayer->FindIndex(m_colStrLayComid, pRight->m_reachComid, 0); 
	float availSourceFlow = 0.0f; // m3/sec
	
	if ( pReach != NULL ) // A Point of Diversion in the Basin being modeled
		{	
		// If min_flow is an attribute in stream layer
		if ( m_colMinFlow != -1 )
			{
			pStreamLayer->GetData(pRight->m_pReach->m_polyIndex, m_colMinFlow, m_minFlow);
			}
		else
			{
			m_minFlow = 0.000001f;
			}		

		//availSourceFlow = streamReachDischargeflow - m_minFlow - instreamWRUse
		availSourceFlow = ( pReach->m_availableDischarge * m_fractionDischargeAvail ) - pReach->m_instreamWaterRightUse; //GetAvailableSourceFlow( pReach );

		if ( availSourceFlow < 0.0 )
			{
			m_unSatInstreamDemand += fabs( availSourceFlow );
         availSourceFlow = 0.0;
			}
		}
   else // if pReach is NULL this is a POD outside of the Basin being modeled.  
		{
		availSourceFlow = 100000.0f;
		}

   if (availSourceFlow <= 0.0f) return(false);

	int wrExists = 0;
	int plantDate = -1;
   int harvDate = -1;
	int nPOUSperIDU = -1;
	int ugb = -1;
	int ugbCol = -1;
	float checkSumMuni = 0;
	float sumMunPodRateIDU = 0.0; // sum of all potential municipal POD rates for this IDU (m3/sec)
	
	float iduAreaHa = 0.f; // hetares
	float iduAreaM2 = 0.0f; // m2
	float iduAreaAc = 0.f;// Acres

	pLayer->GetData(iduNdx, m_colAREA, iduAreaM2);
	pLayer->GetData( iduNdx, m_colWREXISTS,  wrExists );
	pLayer->GetData( iduNdx, m_colPlantDate, plantDate );
   pLayer->GetData( iduNdx, m_colHarvDate,  harvDate );

   unsigned __int16 iduUse = pRight->m_useCode; 
		
	iduAreaHa = iduAreaM2 * HA_PER_M2; 
	
	iduAreaAc = iduAreaM2 * ACRE_PER_M2;

   switch (pRight->m_useCode)
		{

		case WRU_INSTREAM:
/*
			if ( ((availSourceFlow + (pRight->m_podUseRate)) >= m_minFlow) && m_inStreamUseCnt == 1 )
            {
				// if instream use, keep track for relevance to junior water rights, but don't divert any water out of reach.
				// It gets subtracted from availableSourceFlow when computed
				pReach->m_instreamWaterRightUse += pRight->m_podUseRate;
			   m_regulatoryDemand += pRight->m_podUseRate;
            }
*/
			break;

		case WRU_IRRIGATION:

			// This means that a place of use intersects this IDU, thus in pou.csv file. However, it did not meet the area
			// intersected threshold used to define WREXISTS for this IDU
			if ( IsIrrigation(iduUse) )
				{
            //if (pFlowContext->dayOfYear < plantDate || pFlowContext->dayOfYear > harvDate ) break;
				
			   if ( m_wrIrrigateFlag )
				   {

               // m_iduIrrRequestArray is in units mm/day, conversion reference http://www.fao.org/docrep/x0490e/x0490e04.htm
					// ET mm/day = 10 m3/ha/day
					// AREA in IDU layer is assumed to be meters squared m2
					// availSourceFlow = m3/sec
					// 10000 m2/ha
					float iduDuty = 0.0f;
					float totIrrAcreFt = 0.0f;

					float irrRequest = m_iduLocalIrrRequestArray[iduNdx];//mm/d

					if ( irrRequest < 0.0 ) irrRequest = 0.0;

					float iduDemand = irrRequest / 1000.0f * iduAreaM2 / SEC_PER_DAY; //m3/s
					
					pRight->m_stepRequest += iduDemand;
					float iduDemandAcreFt = irrRequest / 1000.0f * iduAreaM2 / M3_PER_ACREFT; // acre-feet

					float actualIrrRequest = m_iduActualIrrRequestDy[iduNdx];

					// just do once per step at the first encounter with this idu
					if ( actualIrrRequest == 0.0 ) m_iduActualIrrRequestDy[iduNdx] = irrRequest;

				  	float totIrrMMYr = m_iduSWIrrArrayYr[iduNdx] + m_iduGWIrrArrayYr[iduNdx]; // mm/day

					if ( totIrrMMYr > 0.0f )
						totIrrAcreFt = totIrrMMYr / 1000.0f * iduAreaM2 / M3_PER_ACREFT; // acre-feet

					if ( totIrrAcreFt > 0.0f )
						iduDuty = totIrrAcreFt / iduAreaAc; // acre-feet per acre

					if ( iduDuty >= m_maxDuty ) 
						{
						int agClass = 0;
						pLayer->GetData(iduNdx, m_colLulc_B, agClass);

						if (m_iduExceedDutyLog[iduNdx] != 1)
							{				
							m_areaDutyExceeds += iduAreaAc;
							
							if ( m_debug )
								{
								if ( agClass == 21 )
									m_anGTmaxDutyArea21 += iduAreaAc;
								if ( agClass == 22 )
									m_anGTmaxDutyArea22 += iduAreaAc;
								if ( agClass == 23)									
									m_anGTmaxDutyArea23 += iduAreaAc;	
								if ( agClass == 24 )
									m_anGTmaxDutyArea24 += iduAreaAc;
								if ( agClass == 25 )
									m_anGTmaxDutyArea25 += iduAreaAc;
								if ( agClass == 26 )
									m_anGTmaxDutyArea26 += iduAreaAc;
								if ( agClass == 27 )
									{	
									m_anGTmaxDutyArea27 += iduAreaAc;
									int iduIndex=-1;
									pLayer->GetData( iduNdx, m_colIDUIndex,iduIndex);
									m_pastureIDUGTmaxDuty = iduIndex;
									m_pastureIDUGTmaxDutyArea = iduAreaM2;
									}
								if ( agClass == 28 )
									m_anGTmaxDutyArea28 += iduAreaAc;
								if ( agClass == 29 )
									m_anGTmaxDutyArea29 += iduAreaAc;
								}						
							}

						m_iduExceedDutyLog[iduNdx] = 1;

						// this will halt irrigation to IDU if max duty is exceeded, and the switch do to so is set in the .xml file
						if ( m_maxDutyHalt == 1 ) 
							{
							m_demandDutyExceeds += iduDemandAcreFt;
							break;
							}
						}

			   	if ( alglib::fp_eq( iduDemand, 0.0 ) )
						{
						// POD rate m3/sec * areal percentage of the POU that intersects the IDU
						m_iduSWUnAllocatedArray[iduNdx]   += pRight->m_podRate * pctPou;
						m_SWUnExIrr += pRight->m_podRate * pctPou;
						m_iduSWUnExerIrrArrayDy[iduNdx] += pRight->m_podRate * pctPou;
						m_SWunallocatedIrrWater += pRight->m_podRate * pctPou;
						break;
						}

					// if available source flow is greater than half of the demand, satisfy all of demand
					if ( availSourceFlow >= iduDemand )
						{					
						// add a negative flux (sink) to the reach. converting from m3/sec to m3/day
						float flux = iduDemand * SEC_PER_DAY;

						pReach->AddFluxFromGlobalHandler( -flux ); //m3/d
						pReach->m_availableDischarge -= iduDemand;

						if ((pRight->m_podRate * pctPou) > iduDemand)
							{
							m_SWUnExIrr += (pRight->m_podRate * pctPou) - iduDemand;
							m_iduSWUnExerIrrArrayDy[iduNdx] += (pRight->m_podRate * pctPou) - iduDemand;
							}

						if ( m_iduSWIrrArrayDy[iduNdx] > 0.0f && irrRequest > 0.0f )
							m_SWIrrAreaDy += iduAreaAc;

						//daily allocated and unallocated ag water m3/sec 
						m_iduSWIrrArrayDy[iduNdx] += irrRequest;// mm/d for a map
						m_iduSWIrrArrayYr[iduNdx] += irrRequest;// mm/d for a map					
						
						m_SWIrrWater += iduDemand;   // m3/s for graph
						m_SWIrrWaterMmDy += irrRequest;   // mm/d for graph 
						m_irrigatedWaterYr += iduDemandAcreFt; // acre-feet
						m_irrigatedSurfaceYr += iduDemandAcreFt; // acre-feet
						m_SWIrrDuty += iduDemand * SEC_PER_DAY; //m3 for graph

                  int tempNPods = m_nIrrPODsPerIDUYr[iduNdx];
						tempNPods++;
						m_nIrrPODsPerIDUYr[iduNdx] = tempNPods;
												
						m_pWaterAllocation->m_iduIrrAllocationArray[iduNdx] += iduDemand; // m3/s
						m_IrrFromAllocationArrayDy += iduDemand * 1000 / iduAreaM2 * SEC_PER_DAY; //mm/day

						//sum the irrigation addition for each polygon to arrive at the total yearly irrigation amount
						m_pWaterAllocation->m_iduYrIrrigArray[iduNdx] += irrRequest; //mm/d;

                  m_iduLocalIrrRequestArray[iduNdx] -= irrRequest; // mm/day
                  
						}
					else
						{ // the available source flow is less demand, allocate all of the available source flow
						
						pStreamLayer->m_readOnly=false;                  
                  pStreamLayer->SetData(pRight->m_pReach->m_polyIndex, m_colWRConflictYr, 1);
						pStreamLayer->SetData(pRight->m_pReach->m_polyIndex, m_colWRConflictDy, 1);
						pStreamLayer->m_readOnly=true;			

						//add a negative value to the reach. converting from m3/sec to m3/day
						float flux = availSourceFlow * SEC_PER_DAY;
						
						float ratio = 0.000001f;

						if ( iduDemand > 0.0 ) 
							ratio = availSourceFlow / iduDemand ;

						pReach->AddFluxFromGlobalHandler(-flux); // m3/day
						
						pReach->m_availableDischarge = 0.0f; // for the rest of the day

						// availableSourceFlow < request , so allocate all of available source flow, so irrRequest = available source flow;
						float irrRequest = availSourceFlow * 1000 / iduAreaM2 * SEC_PER_DAY; //mm/day

						//daily allocated irrigation water mm/d 
						m_iduSWIrrArrayDy[iduNdx] += irrRequest;// mm/d for a map
						m_iduSWIrrArrayYr[iduNdx] += irrRequest;// mm/d for a map

						if ( m_iduGWIrrArrayDy[iduNdx] > 0.0f && irrRequest > 0.0f )
							m_SWIrrAreaDy += iduAreaAc;		
						
						m_SWIrrDuty += availSourceFlow * SEC_PER_DAY; //m3 for graph

						m_SWIrrWater += availSourceFlow; // m3/sec for graph
						m_SWIrrWaterMmDy += irrRequest; // mm/d for graph
						m_irrigatedWaterYr += availSourceFlow * SEC_PER_DAY / M3_PER_ACREFT; // acre-feet
						m_irrigatedSurfaceYr += availSourceFlow * SEC_PER_DAY / M3_PER_ACREFT; // acre-feet

                  int tempNPods = m_nIrrPODsPerIDUYr[iduNdx];
						tempNPods++;
						m_nIrrPODsPerIDUYr[iduNdx] = tempNPods;
						
						m_pWaterAllocation->m_iduIrrAllocationArray[iduNdx] += availSourceFlow; //m3/sec
						m_IrrFromAllocationArrayDy += availSourceFlow * 1000 / iduAreaM2 * SEC_PER_DAY; //mm/day

						//sum the irrigation addition for each polygon to arrive at the total yearly irrigation amount
						m_pWaterAllocation->m_iduYrIrrigArray[iduNdx] += irrRequest; //mm/d;
					
						m_iduLocalIrrRequestArray[iduNdx] -= availSourceFlow * 1000 / iduAreaM2 * SEC_PER_DAY; //mm/day
                  
						float deficit = iduDemand - availSourceFlow;

						ApplyRegulation( pFlowContext, m_regulationType, pRight, m_recursiveDepth, deficit );
                  /*
						pStreamLayer->m_readOnly=false;
						pStreamLayer->SetData(pRight->m_pReach->m_polyIndex, m_colWRConflictYr, 2);
						pStreamLayer->SetData(pRight->m_pReach->m_polyIndex, m_colWRConflictDy, 2);
						pStreamLayer->m_readOnly=true;						
						int index = pStreamLayer->FindIndex(m_colStrLayComid, pRight->m_reachComid, 0);
							
						m_dailyConflictCnt[index] += 1; // how many times reach is unable to satisfy irrigation request

						if ( m_dailyConflictCnt[index] == 1 )
							m_reachDaysInConflict[index] += 1; // how many times per day reach is unable to satisfy irrigation request

						int nDaysInConflict = m_reachDaysInConflict[index];
						pStreamLayer->m_readOnly=false;
						pStreamLayer->SetData(index, m_colNInConflict, nDaysInConflict);
						pStreamLayer->m_readOnly=true;
                  */
                  
						} // end of: if ( !shutoffFlag && availSourceFlow >= iduDemand ) else if (!shutoffFlag )
   			   }  // end of: if (m_wIrrigFlag )
	   		}  // end of: if ( IsIrrig() )
			break;

		case WRU_MUNICIPAL:

			// This IDU do not have a municipal Water right then break         
			if ( IsMunicipal(iduUse) )
				{
   			ugbCol = pLayer->GetFieldCol("UGB");
   			ugb = -1;  
   		   pLayer->GetData( iduNdx, ugbCol, ugb );

            if (1 <= ugb && ugb <= MAX_UGA_NDX)
               {
               CString msg;
               msg.Format("*** AllocateSurfaceWR() ugb = %d, iduNdx = %d, IsMunicipal(iduUse) = %d, iduUse = %d",
                  ugb, iduNdx, IsMunicipal(iduUse), iduUse);
               Report::Log(msg);
               break;
               }

   			nPOUSperIDU = GetMuni(wrExists);

				// m_iduIrrRequestArray is in units mm/day, conversion reference http://www.fao.org/docrep/x0490e/x0490e04.htm
				// ET mm/day = 10 m3/ha/day
				// AREA in IDU layer is assumed to be meters squared m2
				// availSourceFlow = m3/sec
				// 10000 m2/ha

				float iduAreaHa = 0.f;
				float iduAreaM2 = 0.f;
				float iduDemand = 0.f;

				pLayer->GetData(iduNdx, m_colAREA, iduAreaM2); // m2

				pLayer->GetData(iduNdx, m_colUDMAND_DY, iduDemand); // m3/sec

				//pRight->m_stepRequest = iduDemand;

				//m2 -> ha
				iduAreaHa = iduAreaM2 * HA_PER_M2;

				if ( iduDemand <= 0.f )
					{
					// POD rate m3/sec * areal percentage of the POU that intersects the IDU
					m_iduSWUnAllocatedArray[iduNdx] += pRight->m_podRate * pctPou;
					m_SWUnExMun += pRight->m_podRate * pctPou;
					m_unallocatedMuniWater += pRight->m_podRate * pctPou;
					m_iduSWUnExerMunArrayDy[iduNdx] += pRight->m_podRate * pctPou; 
					break;
					}

				// if available source flow is greater than or equal to the demand, satisfy all of demand
				if ( availSourceFlow >= iduDemand )
					{
					// add a negative flux (sink) to the reach. converting from m3/sec to m3/day
					float flux = iduDemand * SEC_PER_DAY;

					pReach->AddFluxFromGlobalHandler(-flux); //m3/d
					pReach->m_availableDischarge -= iduDemand;
               
					m_iduSWUnAllocatedArray[iduNdx] += availSourceFlow - iduDemand;  // m3/sec?

					if ((pRight->m_podRate * pctPou) > iduDemand)
						{					
						m_SWUnExMun += ( pRight->m_podRate * pctPou ) - iduDemand;
						m_iduSWUnExerMunArrayDy[iduNdx] += ( pRight->m_podRate * pctPou ) - iduDemand;
						}

					// daily allocated and unallocated municipal water m3/sec    
					m_iduSWMuniArrayDy[iduNdx] += iduDemand ; // m3/sec for a map
               m_iduSWMuniArrayYr[iduNdx] += iduDemand * (60 * 60 * 24); // m3 H2O

					//m_SWMuniWater += iduDemand ; // m3/sec for graph               
					m_SWMuniWater += iduDemand ; // m3/sec for graph 
					
					m_pWaterAllocation->m_iduNonIrrAllocationArray[iduNdx] += iduDemand ; // m3/s
					//sum the irrigation addition for each polygon to arrive at the total yearly irrigation amount
					m_pWaterAllocation->m_iduYrMuniArray[iduNdx] += iduDemand ; //m3/s;

					}
				else // the available source flow is less than demand, allocate all of the available source flow
					{
					// add a negative value to the reach. converting from m3/sec to m3/day
					float flux = availSourceFlow * SEC_PER_DAY;
					float ratio = 0.000001f;
					
					if ( iduDemand > 0.0 )
						ratio = availSourceFlow / iduDemand;

					pReach->AddFluxFromGlobalHandler(-flux); // m3/day
					pReach->m_availableDischarge = 0.0f;

					m_iduSWUnsatMunDmdYr[iduNdx] += iduDemand - availSourceFlow;

               m_iduSWUnSatMunArrayDy[iduNdx] += iduDemand - availSourceFlow;

					m_SWMunUnSatDemand += iduDemand - availSourceFlow;

					if (( pRight->m_podRate * pctPou ) > iduDemand)
						{
						m_iduSWUnExerMunArrayDy[iduNdx] += ( pRight->m_podRate * pctPou ) - availSourceFlow;
						m_SWUnExMun += ( pRight->m_podRate * pctPou ) - availSourceFlow;
						}

				   m_iduSWMuniArrayDy[iduNdx] += availSourceFlow; // m3/sec for a map
               m_iduSWMuniArrayYr[iduNdx] += availSourceFlow * (60 * 60 * 24); // m3 H2O

					//m_SWMuniWater += availSourceFlow ; // m3/sec for graph
					m_SWMuniWater += availSourceFlow; // m3/sec for graph
					
					m_pWaterAllocation->m_iduNonIrrAllocationArray[iduNdx] += availSourceFlow; // m3/s
					//sum the irrigation addition for each polygon to arrive at the total yearly irrigation amount
					m_pWaterAllocation->m_iduYrMuniArray[iduNdx] += availSourceFlow; // m3/s;

					/*log conflict if availabe source less  demand
					int depth = 0;  // move to .xml

					float deficit = iduDemand - availSourceFlow;

					int nJuniorsAffected = ApplyRegulation( pFlowContext, m_regulationType, pRight, m_recursiveDepth, deficit );

					LogWeeklySurfaceWaterMunicipalConflicts(pFlowContext, pRight, iduNdx, areaPou);
               */
					}
				}

			break;

		default: // ignore everything else

			break;

		}

	return true;
   } // end of AllocateSurfaceWR()

bool AltWaterMaster::AllocateWellWR(FlowContext *pFlowContext, WaterRight *pRight, int iduNdx, float pctPou, float areaPou)
	{
	MapLayer *pLayer = (MapLayer*)pFlowContext->pEnvContext->pMapLayer;

	int wrExists = 0;
	int plantDate = -1;
   int harvDate = -1;
	int nPOUSperIDU = -1;
	int ugb = -1;
	int ugbCol = -1;
	float checkSumMuni = 0;
	float sumMunPodRateIDU = 0.0; // sum of all potential municipal POD rates for this IDU (m3/sec)
	float iduAreaHa = 0.f;
	float iduAreaM2 = 0.0f;
	float iduAreaAc = 0.f;

	pLayer->GetData(iduNdx, m_colAREA, iduAreaM2);
	pLayer->GetData( iduNdx, m_colWREXISTS,  wrExists );
	pLayer->GetData( iduNdx, m_colPlantDate, plantDate );
   pLayer->GetData( iduNdx, m_colHarvDate,  harvDate );

	//m2 -> ha
	iduAreaHa = iduAreaM2 * HA_PER_M2;
	// m2 -> acres
	iduAreaAc = iduAreaM2 * ACRE_PER_M2;
	
	unsigned __int16 iduUse = pRight->m_useCode; 

	//*************************** Begin Temporary for development **********************************************
	// Assumes that irrigation source from well is infinite
	float availSourceFlow = 1000000.0; // m3/sec
	float wellHead = 0.0; // generic for now, may have to define later. 
	//*************************** End Temporary for development ************************************************

   switch (pRight->m_useCode)
		{

		case WRU_IRRIGATION:

			// This means that a place of use intersects this IDU, thus in pou.csv file. However, it did not meet the area
			// intersected threshold used to define WREXISTS for this IDU
			if ( IsIrrigation(iduUse) )
				{
				//if (pFlowContext->dayOfYear < plantDate || pFlowContext->dayOfYear > harvDate ) break;

				if ( m_wrIrrigateFlag )
					{
                    // m_iduIrrRequestArray is in units mm/day, conversion reference http://www.fao.org/docrep/x0490e/x0490e04.htm
					// ET mm/day = 10 m3/ha/day
					// AREA in IDU layer is assumed to be meters squared m2
					// availSourceFlow = m3/sec
					// 10000 m2/ha

					float iduAreaHa = 0.f;
					float iduAreaM2 = 0.0f;
					float iduDuty = 0.0f;
					float totIrrAcreFt = 0.0f;

					// m2
					pLayer->GetData(iduNdx, m_colAREA, iduAreaM2);

					//m2 -> ha
					iduAreaHa = iduAreaM2 * HA_PER_M2;

					float irrRequest = m_iduLocalIrrRequestArray[iduNdx];//mm/d

					if ( irrRequest < 0.0 ) irrRequest = 0.0;

					// m3/sec = m3/ha/day * ha / sec/day
					// float iduDemand = m_iduLocalIrrRequestArray[ iduNdx ] * iduAreaHa / SEC_PER_DAY;

					float iduDemand = irrRequest / 1000.0f * iduAreaM2 / SEC_PER_DAY; // m3/s
					
					//pRight->m_stepRequest = iduDemand;
					float iduDemandAcreFt = irrRequest / 1000.0f * iduAreaM2 / M3_PER_ACREFT; // acre-feet

					float actualIrrRequest = m_iduActualIrrRequestDy[iduNdx]; // mm/day

					// just do once per step at the first encounter with this idu
					if ( actualIrrRequest == 0.0 ) m_iduActualIrrRequestDy[iduNdx] = irrRequest;

					float totIrrMMYr = m_iduSWIrrArrayYr[iduNdx] + m_iduGWIrrArrayYr[iduNdx]; // mm/day

					if ( totIrrMMYr > 0.0f )
						totIrrAcreFt = totIrrMMYr / 1000.0f * iduAreaM2 / M3_PER_ACREFT; // acre-feet

					if ( totIrrAcreFt > 0.0f )
						iduDuty = totIrrAcreFt / iduAreaAc; // acre-feet per acre

					if ( iduDuty >= m_maxDuty ) 
						{
						int agClass = 0;
						pLayer->GetData(iduNdx, m_colLulc_B, agClass);

						if (m_iduExceedDutyLog[iduNdx] != 1)
							{				
							m_areaDutyExceeds += iduAreaAc;
							
							if ( m_debug )
								{
								if ( agClass == 21 )
									m_anGTmaxDutyArea21 += iduAreaAc;
								if ( agClass == 22 )
									m_anGTmaxDutyArea22 += iduAreaAc;
								if ( agClass == 23 )																									
									m_anGTmaxDutyArea23 += iduAreaAc;	
								if ( agClass == 24 )
									m_anGTmaxDutyArea24 += iduAreaAc;
								if ( agClass == 25 )
									m_anGTmaxDutyArea25 += iduAreaAc;
								if ( agClass == 26 )
									m_anGTmaxDutyArea26 += iduAreaAc;
								if (agClass == 27)
									{	
									m_anGTmaxDutyArea27 += iduAreaAc;
									int iduIndex=-1;
									pLayer->GetData( iduNdx, m_colIDUIndex,iduIndex);
									m_pastureIDUGTmaxDuty = iduIndex;
									m_pastureIDUGTmaxDutyArea = iduAreaM2;
									}
								if ( agClass == 28 )
									m_anGTmaxDutyArea28 += iduAreaAc;
								if ( agClass == 29 )
									m_anGTmaxDutyArea29 += iduAreaAc;
								}						
							}

						m_iduExceedDutyLog[iduNdx] = 1;

						// this will halt irrigation to IDU if max duty is exceeded, and the switch do to so is set in the .xml file
						if ( m_maxDutyHalt == 1 ) 
							{
							m_demandDutyExceeds += iduDemandAcreFt;
							break;
							}
						}

					float weightFactor = 1.0f;
											
					// if available source flow is greater than half of the demand, satisfy all of demand
					if ( availSourceFlow >= iduDemand  )
						{					
						// add a negative flux (sink) to the reach. converting from m3/sec to m3/day
						float flux = ( iduDemand * SEC_PER_DAY ) ;

						m_iduGWUnAllocatedArray[iduNdx] += availSourceFlow - (iduDemand / pRight->m_nPODSperWR);

						if ( ( pRight->m_podRate * pctPou ) > ( iduDemand )  )
							m_iduGWUnExerIrrArrayDy[iduNdx] += (pRight->m_podRate * pctPou) - ( iduDemand  );

						// only sum area for this IDU if it has not been previously irrigated   
						if ( m_iduGWIrrArrayDy[iduNdx] > 0.0f && irrRequest > 0.0f )
							m_GWIrrAreaDy += iduAreaAc; // acre
						
						//daily allocated and unallocated ag water mm/d 
						m_iduGWIrrArrayDy[iduNdx] += irrRequest ;// mm/d for a map
						m_iduGWIrrArrayYr[iduNdx] += irrRequest ;// mm/d for a map
						m_GWIrrDuty += iduDemand   * SEC_PER_DAY; //m3 for graph
											 
						m_GWIrrWater += iduDemand ;   // m3/s for graph
						m_GWIrrWaterMmDy += irrRequest ; // mm/d for graph
						m_irrigatedWaterYr += iduDemandAcreFt ; // acre-feet
						m_irrigatedGroundYr += iduDemandAcreFt ; // acre-feet
						int tempNPods = m_nIrrPODsPerIDUYr[iduNdx];
						tempNPods++;
						m_nIrrPODsPerIDUYr[iduNdx] = tempNPods;

						m_pWaterAllocation->m_iduIrrAllocationArray[iduNdx] += iduDemand ; // m3/s
						m_IrrFromAllocationArrayDy += iduDemand  * 1000 / iduAreaM2 * SEC_PER_DAY; //mm/day

						//sum the irrigation addition for each polygon to arrive at the total yearly irrigation amount
						m_pWaterAllocation->m_iduYrIrrigArray[iduNdx] += irrRequest ; //mm/d;

                  m_iduLocalIrrRequestArray[iduNdx] -= irrRequest; //mm/day

						}
					else // the available source flow is less than demand, allocate all of the available source flow
						{
						//add a negative value to the reach. converting from m3/sec to m3/day
						float flux = availSourceFlow * SEC_PER_DAY;
						float ratio = 0.000001f;
						
						if ( iduDemand > 0.0 ) 
							ratio = availSourceFlow / iduDemand;

						m_iduGWUnsatIrrReqYr[iduNdx] += ( iduDemand  ) - availSourceFlow;

						m_GWIrrUnSatDemand += (iduDemand ) - availSourceFlow;

						m_unSatisfiedIrrGroundYr += ((iduDemand ) - availSourceFlow)* SEC_PER_DAY / M3_PER_ACREFT; // acre-feet
						
						float unsatDemandM3Sec = ( iduDemand  ) - availSourceFlow; // m3/day

						float unsatDemandMMDY = unsatDemandM3Sec * 1000 / iduAreaM2 * SEC_PER_DAY; //mm/day
						
						if ((pRight->m_podRate * pctPou) > (iduDemand)  )
							m_iduGWUnExerIrrArrayDy[iduNdx] += (pRight->m_podRate * pctPou) - ( iduDemand  );

						if ( m_iduGWIrrArrayDy[iduNdx] > 0.0f && irrRequest > 0.0f )
							m_GWIrrAreaDy += iduAreaAc; // acre

						float irrRequest = availSourceFlow * 1000 / iduAreaM2 * SEC_PER_DAY; //mm/day

						//daily allocated irrigation water m3/sec 
						m_iduGWIrrArrayDy[iduNdx] += irrRequest;// mm/d for a map
						m_iduGWIrrArrayYr[iduNdx] += irrRequest;// mm/d for a map
						m_GWIrrDuty += availSourceFlow * SEC_PER_DAY; //m3 for graph
						
						m_GWIrrWater += availSourceFlow;   // m3/s for graph
						m_GWIrrWaterMmDy += irrRequest; // mm/d for graph
						m_irrigatedWaterYr += availSourceFlow * SEC_PER_DAY / M3_PER_ACREFT; // acre-feet
						m_irrigatedGroundYr += availSourceFlow * SEC_PER_DAY / M3_PER_ACREFT; // acre-feet
						int tempNPods = m_nIrrPODsPerIDUYr[iduNdx];
						tempNPods++;
						m_nIrrPODsPerIDUYr[iduNdx] = tempNPods;

						m_pWaterAllocation->m_iduIrrAllocationArray[iduNdx] += availSourceFlow; // m3/s
						m_IrrFromAllocationArrayDy += availSourceFlow * 1000 / iduAreaM2 * SEC_PER_DAY; //mm/day

						//sum the irrigation addition for each polygon to arrive at the total yearly irrigation amount
						m_pWaterAllocation->m_iduYrIrrigArray[iduNdx] += irrRequest; //mm/d;

						m_iduLocalIrrRequestArray[iduNdx] -= irrRequest; //mm/day
						}
					}
				}
			
			break;

		case WRU_MUNICIPAL:

			// This IDU do not have a municipal Water right then break         
			if ( IsMunicipal(iduUse) )
				{
			
				ugbCol = pLayer->GetFieldCol("UGB");
				ugb = -1;  
				pLayer->GetData( iduNdx, ugbCol, ugb );

            if (1 <= ugb && ugb <= MAX_UGA_NDX)
               {
               CString msg;
               msg.Format("*** AllocateSurfaceWR() ugb = %d, iduNdx = %d, IsMunicipal(iduUse) = %d, iduUse = %d",
                  ugb, iduNdx, IsMunicipal(iduUse), iduUse);
               Report::Log(msg);
               break;
               }

				nPOUSperIDU = GetMuni(wrExists);

				// m_iduIrrRequestArray is in units mm/day, conversion reference http://www.fao.org/docrep/x0490e/x0490e04.htm
				// ET mm/day = 10 m3/ha/day
				// AREA in IDU layer is assumed to be meters squared m2
				// availSourceFlow = m3/sec
				// 10000 m2/ha

				float iduAreaHa = 0.f;
				float iduAreaM2 = 0.f;
				float iduDemand = 0.f;

				pLayer->GetData(iduNdx, m_colAREA, iduAreaM2); // m2
           
            pLayer->GetData(iduNdx, m_colUDMAND_DY, iduDemand); // m3/sec

				//pRight->m_stepRequest = iduDemand;
				//m2 -> ha
				iduAreaHa = iduAreaM2 * HA_PER_M2;

				if ( iduDemand <= 0.f )
					{
					// POD rate m3/sec * areal percentage of the POU that intersects the IDU
					m_iduGWUnAllocatedArray[iduNdx]   += pRight->m_podRate * pctPou;
					m_iduGWUnExerMunArrayDy[iduNdx] += pRight->m_podRate * pctPou;
					m_unallocatedMuniWater += pRight->m_podRate * pctPou;
					//break;
					}

				// if available source flow is greater than demand, satisfy all of demand
				if (availSourceFlow >= (iduDemand  / 2 ) )
					{
					// add a negative flux (sink) to the reach. converting from m3/sec to m3/day
					float flux = (iduDemand * SEC_PER_DAY) ;

					m_iduGWUnAllocatedArray[iduNdx] += availSourceFlow - ( iduDemand  );  // m3/sec?

					if  ((pRight->m_podRate * pctPou) > (iduDemand)  )
						m_iduGWUnExerMunArrayDy[iduNdx] += ( pRight->m_podRate * pctPou ) - ( iduDemand  );

					// daily allocated and unallocated municipal water m3/sec    
					m_iduGWMuniArrayDy[iduNdx] += iduDemand ; // m3/sec for a map
					m_iduGWMuniArrayYr[iduNdx] += (iduDemand)*(60*60*24) ; // m3 H2O
					
					//m_GWMuniWater += iduDemand ; // m3/sec for graph               
					m_GWMuniWater += iduDemand ; // m3/sec for graph 
					
					m_pWaterAllocation->m_iduNonIrrAllocationArray[iduNdx] += iduDemand ; // m3/s
					//sum the irrigation addition for each polygon to arrive at the total yearly irrigation amount
					m_pWaterAllocation->m_iduYrMuniArray[iduNdx] += iduDemand ; //m3/sec;

					}
				else // the available source flow is less than demand, allocate all of the available source flow
					{
					// add a negative value to the reach. converting from m3/sec to m3/day
					float flux = availSourceFlow * SEC_PER_DAY;
					float ratio = 0.000001f;
					
					if ( iduDemand > 0.0 )
						ratio = availSourceFlow / iduDemand;

					m_iduGWUnsatMunDmdYr[iduNdx] += ( iduDemand  ) - availSourceFlow;
					
					m_iduGWUnSatMunArrayDy[iduNdx] += ( iduDemand  ) - availSourceFlow;
					
					m_GWMunUnSatDemand += (iduDemand ) - availSourceFlow;

					if ( (pRight->m_podRate * pctPou) > (iduDemand)  )
						m_iduGWUnExerMunArrayDy[iduNdx] += ( pRight->m_podRate * pctPou ) - ( iduDemand  );
					
					m_iduGWMuniArrayDy[iduNdx] += availSourceFlow; // m3/sec for a map
               m_iduGWMuniArrayYr[iduNdx] += availSourceFlow * SEC_PER_DAY; // m3 H2O

					//m_GWMuniWater += availSourceFlow ; // m3/sec for graph
					m_GWMuniWater += availSourceFlow; // m3/sec for graph
					
					m_pWaterAllocation->m_iduNonIrrAllocationArray[iduNdx] += availSourceFlow; // m3/s
					//sum the irrigation addition for each polygon to arrive at the total yearly irrigation amount
					m_pWaterAllocation->m_iduYrMuniArray[iduNdx] += iduDemand; // m3/s;

					//log conflict if availabe source less than half demand
					if ( availSourceFlow <= ( iduDemand  / 2 ) )
						LogWeeklySurfaceWaterIrrigationConflicts(pFlowContext, pRight, iduNdx, areaPou);
					}
				}

			break;

		default: // ignore everything else

			break;

		}

	return true;
}

bool AltWaterMaster::LogWeeklySurfaceWaterIrrigationConflicts(FlowContext *pFlowContext, WaterRight *pRight, int iduNdx, float areaPou)
   {
   CString msg;
   msg.Format("LogWeeklySurfaceWaterIrrigationConflicts() was called from AllocateWellWR()"); 
   Report::LogWarning(msg);
   return(false);
   }

bool AltWaterMaster::AggregateIDU2HRU(FlowContext *pFlowContext)
	{
	int hruCount = pFlowContext->pFlowModel->GetHRUCount();
   MapLayer  *pIDULayer = (MapLayer*) pFlowContext->pEnvContext->pMapLayer;

	int iduNdx;

	float hruWaterAllotment;

//#pragma omp parallel for
   for (int h = 0; h < hruCount; h++)
   {
      hruWaterAllotment = 0.0f;

      HRU *pHRU = pFlowContext->pFlowModel->GetHRU(h);

      HRUPool *pIrrHRULayer = pHRU->GetPool( m_irrigatedHruLayer ); //use "irrigated" layer

		HRUPool *pMunHRULayer = pHRU->GetPool( m_nonIrrigatedHruLayer ); // use "non-irrigated layer"  

      int polyCount = int(pHRU->m_polyIndexArray.GetCount());

      for (int p = 0; p < polyCount; p++)
      {

         iduNdx = pHRU->m_polyIndexArray[p];
         
			float irrFlux = m_pWaterAllocation->m_iduIrrAllocationArray[iduNdx] * SEC_PER_DAY; // m3/day
			float munFlux = m_pWaterAllocation->m_iduNonIrrAllocationArray[iduNdx] * SEC_PER_DAY; // m3/day
         
         if ( irrFlux > 0.0 ) 
				pIrrHRULayer->AddFluxFromGlobalHandler(irrFlux, FL_TOP_SOURCE); //m3/d - add flux to top of soil
			if ( munFlux > 0.0 ) 
				pMunHRULayer->AddFluxFromGlobalHandler(munFlux, FL_TOP_SOURCE); //m3/d - add flux to top of soil
      }
   }

	return true;
	}



void AltWaterMaster::LogWeeklySurfaceWaterIrrigationShortagesByIDU(int iduNdx, bool shortageFlag )
	{
   if (shortageFlag)
      {
      m_iduConsecutiveShortages[iduNdx]++;
      if (m_iduSWIrrWRSOIndex[iduNdx] == 1 && m_iduConsecutiveShortages[iduNdx] >= m_nDaysWRConflict2) m_iduSWIrrWRSOIndex[iduNdx] = 2;
      else if (m_iduSWIrrWRSOIndex[iduNdx] < 1 && m_iduConsecutiveShortages[iduNdx] >= m_nDaysWRConflict1) m_iduSWIrrWRSOIndex[iduNdx] = 1;
      }
   else m_iduConsecutiveShortages[iduNdx] = 0;
	} // end of LogWeeklySurfaceWaterIrrigationShortagesByIDU()

	bool AltWaterMaster::LogWeeklySurfaceWaterMunicipalConflicts(FlowContext *pFlowContext, WaterRight *pRight, int iduNdx, float areaPou)
	{
	// available water is below minimum. CONFLICT  Water Right Shut Off (WRSO).
	// Narative: a.	Weeks that water deliveries are shut off in a year.  
	// One delivery of one water-right is shut-off in one week, the index = 1. 
	// If the water right is shut off again in a subsequent week the index would be 2.  
	// 2 would also be 2 water rights shut off in one week. 

	MapLayer *pLayer = (MapLayer*)pFlowContext->pEnvContext->pMapLayer;

	// number days shut off (SO)
	pRight->m_nDaysPerWeekSO++;

	// at the end of each week, check for conflict
	if ( pFlowContext->dayOfYear == m_weekInterval )
		{
		if ( pRight->m_nDaysPerWeekSO == m_nDaysWRConflict1 ) //defined in .xml
			{
			m_iduSWMunWRSOIndex[iduNdx] = 1;
			m_iduSWMunWRSOWeek[iduNdx] = pFlowContext->dayOfYear;
			m_pouSWLevelOneMunWRSOAreaArray[iduNdx] = areaPou;
			pLayer->GetData( iduNdx, m_colAREA, m_iduSWLevelOneMunWRSOAreaArray[iduNdx] );
			}

		if ( pRight->m_nDaysPerWeekSO >= m_nDaysWRConflict2 ) //defined in .xml
			{
			m_iduSWMunWRSOIndex[iduNdx] = 2;
			m_iduSWMunWRSOWeek[iduNdx] = pFlowContext->dayOfYear;
			m_pouSWLevelTwoMunWRSOAreaArray[iduNdx] = areaPou;
			pLayer->GetData( iduNdx, m_colAREA, m_iduSWLevelTwoMunWRSOAreaArray[iduNdx] );
			}

		// reset number of days per week shut off to zero
		pRight->m_nDaysPerWeekSO = 0;

		}

	return true;
	}


bool AltWaterMaster::EndStep(FlowContext *pFlowContext)
{
	MapLayer *pLayer = (MapLayer*)pFlowContext->pEnvContext->pMapLayer;
   MapLayer* pStreamLayer = (MapLayer*)pFlowContext->pFlowModel->m_pStreamLayer;

	float time = pFlowContext->time - pFlowContext->pEnvContext->startYear * 365;

   m_fromOutsideBasinYr += m_fromOutsideBasinDy * SEC_PER_DAY;

	if ( m_debug )
		{
		CArray< float, float > rowDailyMetricsDebug;
		
		rowDailyMetricsDebug.SetSize(10);

		rowDailyMetricsDebug[0] = time;
		rowDailyMetricsDebug[1] = m_dyGTmaxPodArea21;
		rowDailyMetricsDebug[2] = m_dyGTmaxPodArea22;
		rowDailyMetricsDebug[3] = m_dyGTmaxPodArea23;
		rowDailyMetricsDebug[4] = m_dyGTmaxPodArea24;
		rowDailyMetricsDebug[5] = m_dyGTmaxPodArea25;
		rowDailyMetricsDebug[6] = m_dyGTmaxPodArea26;
		rowDailyMetricsDebug[7] = m_dyGTmaxPodArea27;
		rowDailyMetricsDebug[8] = m_dyGTmaxPodArea28;
		rowDailyMetricsDebug[9] = m_dyGTmaxPodArea29;
		this->m_dailyMetricsDebug.AppendRow(rowDailyMetricsDebug);

		m_dyGTmaxPodArea21 =0.0f;
		m_dyGTmaxPodArea22 =0.0f;
		m_dyGTmaxPodArea23 =0.0f;
		m_dyGTmaxPodArea24 =0.0f;
		m_dyGTmaxPodArea25 =0.0f;
		m_dyGTmaxPodArea26 =0.0f;
		m_dyGTmaxPodArea27 =0.0f;
		m_dyGTmaxPodArea28 =0.0f;
		m_dyGTmaxPodArea29 =0.0f;
		}

		// Reset a few things associated with a WR.
		for ( int i = 0; i < (int)m_podArray.GetSize(); i++ )
   		{
			// Get current WR or POD
			WaterRight *pRight = m_podArray[i];
			pRight->m_stepShortageFlag = false;
			}


		
	// write values to map
	MapLayer *pIDULayer = (MapLayer*)pFlowContext->pEnvContext->pMapLayer;

	int iduCount = pIDULayer->GetRecordCount();
	int colArea = pIDULayer->GetFieldCol("AREA");
         
	for (MapLayer::Iterator idu = pLayer->Begin(); idu != pLayer->End(); idu++)
   	{
		m_iduIsIrrOutSeasonDy[idu] = 0;

		//float applied = m_pWaterAllocation->m_iduAllocationArray[ i ];  // m3/sec
		float swApplied = m_iduSWIrrArrayDy[idu];   // mm/day
		float gwApplied = m_iduGWIrrArrayDy[idu];   // mm/day
		float deficit   = m_iduLocalIrrRequestArray[idu];
		float fracDemand = 0.0f;
		float actualIrrRequest = m_iduActualIrrRequestDy[idu];
		float allocatedIrr = m_iduSWIrrArrayDy[idu] + m_iduGWIrrArrayDy[idu]; 
		float unexercisedSWirr = m_iduSWUnExerIrrArrayDy[idu];
		float unexercisedSWmun = m_iduSWUnExerMunArrayDy[idu];
		float unsatSWMunDemand = m_iduSWUnSatMunArrayDy[idu];
		float unsatSWIrrRequest = m_iduSWUnSatIrrArrayDy[idu];
		float unsatGWMunDemand = m_iduGWUnSatMunArrayDy[idu];
      int wr_shortg; pIDULayer->GetData(idu, m_colWRShortG, wr_shortg);
      int wr_shutoff; pIDULayer->GetData(idu, m_colWRShutOff, wr_shutoff);
		
		pIDULayer->m_readOnly = false;
		pIDULayer->SetData(idu, m_colSWIrrDy, swApplied);
		pIDULayer->SetData(idu, m_colGWIrrDy, gwApplied);
		pIDULayer->SetData(idu, m_colWaterDeficit, deficit);
		pIDULayer->SetData(idu, m_colActualIrrRequestDy, actualIrrRequest);
		pIDULayer->SetData(idu, m_colDailyAllocatedIrrigation, allocatedIrr);
		pIDULayer->SetData(idu, m_colSWUnexIrr, unexercisedSWirr);
		pIDULayer->SetData(idu, m_colSWUnexMun, unexercisedSWmun);
		pIDULayer->SetData(idu, m_colSWUnSatMunDemandDy, unsatSWMunDemand);
		pIDULayer->SetData(idu, m_colSWUnSatIrrRequestDy, unsatSWIrrRequest);
		pIDULayer->SetData(idu, m_colGWUnSatMunDemandDy, unsatGWMunDemand);
      // The next two are here so that the daily values get picked up for .csv files when requested in HBV.xml.
      pIDULayer->SetData(idu, m_colWRShortG, wr_shortg);
      pIDULayer->SetData(idu, m_colWRShutOff, wr_shutoff);
      pIDULayer->m_readOnly = true;

		int irrDecision = 1;

	   if ( m_colIrrigate != -1 )
			{
			pLayer->GetData(idu, m_colIrrigate, irrDecision);
			}
		else
			{
			irrDecision = 1; //default 1 =  yes irrigate
			}
/*
      if (idu == 46864 && irrDecision ==1)
         {
         CString msg;
         msg.Format("EndStep for IDU 46864: pFlowContext->dayOfYear = %d, actualIrrRequest = %f",
            pFlowContext->dayOfYear, actualIrrRequest);
         Report::Log(msg);
         }
*/
		float iduAreaM2 = 0.0f;

		pLayer->GetData(idu, m_colAREA, iduAreaM2);

      float irrRequest = m_pWaterAllocation->m_iduIrrRequestArray[idu];

		float iduIrrRequest = ( irrRequest / 1000.0f ) * iduAreaM2 / M3_PER_ACREFT; // acre-feet per day

		pIDULayer->m_readOnly=false;
      if (irrDecision == 1)
         {
         bool readOnlyFlag = pLayer->m_readOnly;
         pLayer->m_readOnly = false;
         pIDULayer->SetData(idu, m_colIrrRequestDy, irrRequest);
         pLayer->m_readOnly = readOnlyFlag;
/*       if (idu == 46864)
            {
            CString msg;
            msg.Format("EndStep for IDU 46864: doy = %d, irrRequest = %f",
               pFlowContext->dayOfYear, irrRequest);
            Report::Log(msg);
            }
*/
         }
				//m_pFlowModel->UpdateIDU(pFlowContext->pEnvContext, idu, m_colIrrRequestDy, iduIrrRequest ); //acre - feet per day
      pIDULayer->m_readOnly = true;

		if (alglib::fp_eq(deficit, 0.0))
			fracDemand = -1;
		else
			fracDemand = ((swApplied + gwApplied) / deficit);  // unitless ratio

		float totMuniAllocatedDy = m_iduSWMuniArrayDy[idu] + m_iduGWMuniArrayDy[idu]; // m3/sec
		float totSWMuniAllocatedDy = m_iduSWMuniArrayDy[idu];
      float totGWMuniAllocatedDy = m_iduGWMuniArrayDy[idu];

		pIDULayer->m_readOnly=false;
		pIDULayer->SetData(idu, m_colSWAlloMunicipalDay,totSWMuniAllocatedDy);
      pIDULayer->SetData(idu, m_colGWAlloMunicipalDay, totGWMuniAllocatedDy);
      pIDULayer->m_readOnly = true;

		float totDyIrrmmDy = m_iduSWIrrArrayDy[idu] + m_iduGWIrrArrayDy[idu]; //mm/day

		// m_iduIrrRequestArray is in units mm/day, conversion reference http://www.fao.org/docrep/x0490e/x0490e04.htm
		// ET mm/day = 10 m3/ha/day
		// AREA in IDU layer is assumed to be meters squared m2
		// availSourceFlow = m3/sec
		// 10000 m2/ha

		iduAreaM2 = 0.0f;

		// m2
		pLayer->GetData(idu, m_colAREA, iduAreaM2);

		float totDyIrrAcft = totDyIrrmmDy / 1000.0f * iduAreaM2 / M3_PER_ACREFT; // acre-feet

		if ( totDyIrrAcft >  m_maxTotDailyIrr[idu] ) 
			m_maxTotDailyIrr[idu] = totDyIrrAcft; // acre-ft
		pIDULayer->m_readOnly=false;	
		pIDULayer->SetData(idu, m_colDemandFraction, fracDemand);
		pIDULayer->m_readOnly=true;

      float iduAreaAc = iduAreaM2 * ACRE_PER_M2;

      // additional tracking
      float maxRate = m_maxRate * iduAreaAc;    // m3/sec for IDU area, based on 1/80 cfs/acre standard
      float totalIduIrrRate = ( totDyIrrmmDy / SEC_PER_DAY ) * M_PER_MM * iduAreaM2;  // m3/sec for IDU area for this day

      int irrigated = 0;
      pIDULayer->GetData( idu, this->m_colIrrigate, irrigated );

      if ( irrigated == 1 )
         {
         int startDate = 366;
         pIDULayer->GetData( idu, this->m_colPlantDate, startDate );
   
         int endDate = 0;
         pIDULayer->GetData( idu, this->m_colHarvDate, endDate );

         if ( pFlowContext->dayOfYear >= startDate && pFlowContext->dayOfYear <= endDate )
            {
            float wastedWater =  maxRate - totalIduIrrRate;   // m3/sec of wasted water for the IDU

            if ( totalIduIrrRate > maxRate * 1.1 )
               {
               m_exceededWaterRateDy += (-wastedWater);     // m3/sec
               m_exceededWaterVolYr += ( -wastedWater * SEC_PER_DAY * ACREFT_PER_M3 );    // acre-ft for this day
					m_iduExceededIrr_Yr[idu] += ( -wastedWater * SEC_PER_DAY * ACREFT_PER_M3 );
               m_daysPerYrMaxRateExceeded += iduAreaAc;
					m_iduExceededIrr_Dy[idu] = (-wastedWater);												
               wastedWater = 0;
               }
   
            m_wastedWaterRateDy +=  wastedWater;   // m3/sec, accumlated over irrigated IDUs 
            m_wastedWaterVolYr  +=  wastedWater * SEC_PER_DAY * ACREFT_PER_M3;  // acre-ft for this day
				m_iduWastedIrr_Yr[idu] += wastedWater * SEC_PER_DAY * ACREFT_PER_M3;  // acre-ft for this day
				m_iduWastedIrr_Dy[idu] = wastedWater;			
            }
         }

		pIDULayer->m_readOnly=false;	
		pIDULayer->SetData( idu, m_colExcessIrrDy,m_iduExceededIrr_Dy[idu]);			
		pIDULayer->SetData( idu, m_colWastedIrrDy,m_iduWastedIrr_Dy[idu] );	
		pIDULayer->m_readOnly=true;

      }  // end of: for each ( idu )

	// summary for all irrigation
	CArray< float, float > rowDailyMetrics;
	
	rowDailyMetrics.SetSize(21);

	rowDailyMetrics[0] = time;

	rowDailyMetrics[1] = m_regulatoryDemand;
	rowDailyMetrics[2] = m_SWIrrWater;
	rowDailyMetrics[3] = m_GWIrrWater;
	rowDailyMetrics[4] = m_SWMuniWater;
	rowDailyMetrics[5] = m_GWMuniWater;
	rowDailyMetrics[6] = m_SWIrrAreaDy;
	rowDailyMetrics[7] = m_GWIrrAreaDy;
	rowDailyMetrics[8] = m_SWUnExIrr;
	rowDailyMetrics[9] = m_SWUnExMun;
	rowDailyMetrics[10] = m_SWIrrDuty;
	rowDailyMetrics[11] = m_GWIrrDuty;
	rowDailyMetrics[12] = m_SWIrrWaterMmDy;
	rowDailyMetrics[13] = m_GWIrrWaterMmDy;
	rowDailyMetrics[14] = m_fromOutsideBasinDy;
	rowDailyMetrics[15] = m_SWMunUnSatDemand;
	rowDailyMetrics[16] = m_GWIrrUnSatDemand;
	rowDailyMetrics[17] = m_GWMunUnSatDemand;
   rowDailyMetrics[18] = m_wastedWaterRateDy;
   rowDailyMetrics[19] = m_exceededWaterRateDy;
	rowDailyMetrics[20] = m_IrrWaterRequestDy;
	this->m_dailyMetrics.AppendRow(rowDailyMetrics);

	// keep track of which week of the year we are in for conflict metrics
	if (pFlowContext->dayOfYear == m_weekInterval)
	   {
		m_weekOfYear++;
		m_weekInterval = m_weekInterval + 7;
	}

   // <output in_use = "1" name = "WRB-Discharge (m3/s)" query = "COMID=23735691" value = "reachOutflow" type = "sum" domain = "reach" / >
   int reachNdx = pStreamLayer->FindIndex(m_colStrLayComid, 23735691, 0);
   if (reachNdx >= 0)
      {
      Reach * pReach = pFlowContext->pFlowModel->FindReachFromIndex(reachNdx);
      m_basin_discharge_accumulator_m3 += pReach->m_mvCurrentStreamFlow * (60*60*24);
      }
   else m_basin_discharge_accumulator_m3 = NAN; // This will happen if we're simulating a subbasin instead of the whole study area.
   
   return true;
   } // end of EndStep()

	bool AltWaterMaster::StartYear(FlowContext *pFlowContext)
		{

		MapLayer* pStreamLayer = (MapLayer*)pFlowContext->pFlowModel->m_pStreamLayer;

		MapLayer *pLayer = (MapLayer*)pFlowContext->pEnvContext->pMapLayer;

		if ( m_dynamicWRType == 2 ) DynamicWaterRights( pFlowContext, m_dynamicWRType, 364 );
		m_nSWLevelOneIrrWRSO = 0;
		m_nSWLevelTwoIrrWRSO = 0;
		m_nSWLevelOneMunWRSO = 0;
		m_nSWLevelTwoMunWRSO = 0;
		m_iduSWAreaLevelOneIrrWRSO = 0.f;
		m_pouSWAreaLevelOneIrrWRSO = 0.f;
		m_iduSWAreaLevelTwoIrrWRSO = 0.f;
		m_pouSWAreaLevelTwoIrrWRSO = 0.f;

		m_irrWaterRequestYr = 0.f;
		m_irrigableIrrWaterRequestYr = 0.f;
		m_irrigatedWaterYr = 0.f;
		m_irrigatedSurfaceYr = 0.f;
		m_irrigatedGroundYr = 0.f;
		m_unSatisfiedIrrigationYr = 0.f;

		m_unSatInstreamDemand = 0.f;

      m_wastedWaterVolYr = 0.0f;
      m_exceededWaterVolYr = 0.0f;
      m_daysPerYrMaxRateExceeded = 0.0f;

		m_areaDutyExceeds = 0.0f;
		m_demandDutyExceeds = 0.0f;
		m_demandOutsideBegEndDates = 0.0f;

		m_irrLenWtReachConflictYr = 0.0f;
		m_pastureIDUGTmaxDuty = -1;
	   m_pastureIDUGTmaxDutyArea = 0.0f; 

      m_basin_discharge_accumulator_m3 = 0.f;

		// reset to 0
		for (MapLayer::Iterator idu = pLayer->Begin(); idu != pLayer->End(); idu++)
			{
			pLayer->m_readOnly = false;	
			pLayer->SetData( idu,m_colIrrRequestYr , 0 );
         pLayer->SetData(idu, m_colWRShutOff, 0);
         pLayer->SetData(idu, m_colWRShutOffMun, 0);
         pLayer->SetData(idu, m_colWRShortG, 0);
         pLayer->SetData(idu, m_colWRJuniorAction    , 0 );
         pLayer->m_readOnly = true;
			m_iduWastedIrr_Yr[idu] = 0.0f;
			m_iduExceededIrr_Yr[idu] = 0.0f;

         m_iduSWLevelOneIrrWRSOAreaArray[idu] = 0.0f;
         m_iduSWLevelTwoIrrWRSOAreaArray[idu] = 0.0f;
         m_iduSWIrrWRSOIndex[idu] = -1;
         m_iduSWIrrWRSOWeek[idu] = -1;
         m_iduSWIrrWRSOLastWeek[idu] = -999;
         m_iduUnsatIrrReqst_Yr[idu] = 0.0f;

         /*Initiate arrays for water right conflict metrics and allocation arrays
         m_iduSWLevelOneMunWRSOAreaArray[idu] = 0.0f;
         m_pouSWLevelOneIrrWRSOAreaArray[idu] = 0.0f;
         m_pouSWLevelOneMunWRSOAreaArray[idu] = 0.0f;
         m_iduSWLevelTwoMunWRSOAreaArray[idu] = 0.0f;
         m_pouSWLevelTwoIrrWRSOAreaArray[idu] = 0.0f;
         m_pouSWLevelTwoMunWRSOAreaArray[idu] = 0.0f;
         m_iduSWMunWRSOIndex[idu] = -1;
         m_iduSWMunWRSOWeek[idu] = -1;
         m_iduLocalIrrRequestArray[idu] = 0.0f;
         m_iduSWMunWRSOLastWeek[idu] = -999;
         m_iduAnnualIrrigationDutyArray[idu] = 0.f;
         m_iduIsIrrOutSeasonDy[idu] = 0;
         m_iduActualIrrRequestDy[idu] = 0.0f;

         // surface water irrigation 
         m_iduSWUnAllocatedArray[idu] = 0.0f;
         m_iduSWIrrArrayDy[idu] = 0.0f;
         m_iduSWIrrArrayYr[idu] = 0.0f;
         m_iduSWMuniArrayDy[idu] = 0.0f;
         //m_iduSWUnsatIrrReqYr[idu] = 0.0f;
         m_iduSWUnsatMunDmdYr[idu] = 0.0f;
         m_iduSWAppUnSatDemandArray[idu] = 0.0f;
         m_iduSWUnExerIrrArrayDy[idu] = 0.0f;
         m_iduSWUnExerMunArrayDy[idu] = 0.0f;
         m_iduSWUnSatIrrArrayDy[idu] = 0.0f;
         m_iduSWUnSatMunArrayDy[idu] = 0.0f;
         m_iduGWUnSatIrrArrayDy[idu] = 0.0f;
         m_iduGWUnSatMunArrayDy[idu] = 0.0f;

         // ground water irrigation 
         m_iduGWUnAllocatedArray[idu] = 0.0f;
         m_iduGWIrrArrayDy[idu] = 0.0f;
         m_iduGWIrrArrayYr[idu] = 0.0f;
         m_iduGWMuniArrayDy[idu] = 0.0f;
         m_iduGWMuniArrayYr[idu] = 0.0f;
         m_iduGWUnsatIrrReqYr[idu] = 0.0f;
         m_iduGWUnsatMunDmdYr[idu] = 0.0f;
         m_iduGWAppUnSatDemandArray[idu] = 0.0f;
         m_iduGWUnExerIrrArrayDy[idu] = 0.0f;
         m_iduGWUnExerMunArrayDy[idu] = 0.0f;
         m_iduIrrWaterRequestYr[idu] = 0.0f;

         //total irrigation 
         m_maxTotDailyIrr[idu] = 0.0f;
         m_aveTotDailyIrr[idu] = 0.0f;
         m_nIrrPODsPerIDUYr[idu] = 0;
         m_iduWastedIrr_Yr[idu] = 0.0f;
         m_iduExceededIrr_Yr[idu] = 0.0f;
         m_iduWastedIrr_Dy[idu] = 0.0f;
         m_iduExceededIrr_Dy[idu] = 0.0f;

         //Duty
         m_iduExceedDutyLog[idu] = 0;
*/

      } // end of IDU loop

   int reachCount = pFlowContext->pFlowModel->GetReachCount();
	
	// reset conflict flags in stream layer
	for (int reachIndex = 0; reachIndex < reachCount; reachIndex++)
		{	
		pLayer->m_readOnly = false;	
		pFlowContext->pFlowModel->m_pStreamLayer->SetData(reachIndex, m_colWRConflictYr, 0);
		pFlowContext->pFlowModel->m_pStreamLayer->SetData(reachIndex,  m_colNInConflict, 0);
		m_reachDaysInConflict[reachIndex] = 0;
		pLayer->m_readOnly = true;		
		}

   for (int i = 0; i < (int)m_podArray.GetSize(); i++)
      {
      // Get current WR
      WaterRight *pRight = m_podArray[i];
      pRight->m_stepRequest = 0.f;
      } // end of loop thru POD array

   m_SWIrrAreaYr = 0.0f;
   m_GWIrrAreaYr = 0.0f;
   m_weekInterval = 6; // 0 based, expressed as day of year
   m_daysPerYrMaxRateExceeded = 0.0f;
   pLayer->SetColData(m_colAllocatedIrrigation, VData(0), true);
   for (int idu = 0; idu<m_iduConsecutiveShortages.GetSize(); idu++) m_iduConsecutiveShortages[idu] = 0;
   for (int uga = 0; uga <= MAX_UGA_NDX; uga++)
      {
      m_ugaUWallocatedYr[uga] = 0.f;
      m_ugaUWshortageYr[uga] = 0.f;
      m_ugaUWfromSW[uga] = 0.f;
      m_ugaUWfromGW[uga] = 0.f;
      } // end of loop on UGAs
   m_fromOutsideBasinYr = 0.f;
   m_GWnoWR_Yr_m3 = 0.f;


   return true;
	} // end of AltWaterMaster::StartYear()


bool AltWaterMaster::EndYear(FlowContext *pFlowContext)
   {
	MapLayer *pLayer = (MapLayer*)pFlowContext->pEnvContext->pMapLayer;	
	MapLayer* pStreamLayer = (MapLayer*)pFlowContext->pFlowModel->m_pStreamLayer;

   // accumulators for the Quick Check metrics
   double precip_accumulator_m3 = 0.f; // accumulates m3 H2O
   double groundwater_accumulator_ac_ft = 0.f; // accumulates acre-feet H2O
   double aet_accumulator_m3 = 0.f; // accumulates m3 H2O
   double tot_area = 0.f;
	int iduCount = pLayer->GetRecordCount();
   m_SWIrrAreaYr = 0;
   m_GWIrrAreaYr = 0;
	vector<int> *iduNdxVec = 0;

   int reachCount = pFlowContext->pFlowModel->GetReachCount();

	for (int i = 0; i < reachCount; i++)
		{				
		int nDaysInConflict = m_reachDaysInConflict[i];
      
		if ( nDaysInConflict > 0 )
			{
			float reachLength = 0.0f;

			pStreamLayer->GetData(i, m_colReachLength, reachLength);

			m_irrLenWtReachConflictYr += reachLength/m_basinReachLength;
			}	
		}

   double ugaPop[MAX_UGA_NDX+1]; 
   for (int uga = 0; uga <= MAX_UGA_NDX; uga++) ugaPop[uga] = 0.f;

	for (int idu = 0; idu < iduCount; idu++)
	   {
      int uga = -1; pLayer->GetData(idu, m_colUGB, uga);
      if (uga >= 1 && uga <= MAX_UGA_NDX)
         {
         float idu_pop = 0.f; pLayer->GetData(idu, m_colPOP, idu_pop);
         ugaPop[uga] += idu_pop;
         }

		float iduAreaHa = 0.f;
		float iduAreaM2 = 0.0f;
		float iduAreaAc = 0.f;
		int plantDate = 0;
		int harvDate = 0;
		int iduNdx = -1;

		pLayer->GetData( idu, m_colAREA, iduAreaM2 );
      tot_area += iduAreaM2;

		pLayer->GetData( idu, m_colPlantDate, plantDate );
      pLayer->GetData( idu, m_colHarvDate,  harvDate );
		//m2 -> ha
		iduAreaHa = iduAreaM2 * HA_PER_M2;
		// m2 -> acres
		iduAreaAc = iduAreaM2 * ACRE_PER_M2;

		// Quantify water right shut off metrics
		if ( m_iduSWIrrWRSOIndex[idu] == 1 )
			m_nSWLevelOneIrrWRSO++;

		if ( m_iduSWIrrWRSOIndex[idu] == 2 )
			m_nSWLevelTwoIrrWRSO++;

      if ( !pFlowContext->pFlowModel->m_estimateParameters )
         {
         if (m_iduSWIrrWRSOIndex[idu] == 1)
					 {					 
				    m_pFlowModel->AddDelta( pFlowContext->pEnvContext, idu, m_colWRShortG, 1 );
					 }
				 if (m_iduSWIrrWRSOIndex[idu] == 2)
					 {
				    m_pFlowModel->AddDelta( pFlowContext->pEnvContext, idu, m_colWRShortG, 2 );
					 }
         }

		// check week before for water right shut off, if so, then index = 2
		if ( m_iduSWMunWRSOWeek[idu] == 6 )
		   {
			if ( m_iduSWMunWRSOWeek[idu] - m_iduSWMunWRSOLastWeek[idu] == -358 )
				m_iduSWMunWRSOIndex[idu] = 2;
		   }
		else
		   {
			if ( m_iduSWMunWRSOWeek[idu] - m_iduSWMunWRSOLastWeek[idu] == 7 )
				m_iduSWMunWRSOIndex[idu] = 2;
		   }

		// Quantify water right shut off metrics
		if ( m_iduSWMunWRSOIndex[idu] == 1 )
			m_nSWLevelOneMunWRSO++;

		if ( m_iduSWMunWRSOIndex[idu] == 2 )
			m_nSWLevelTwoMunWRSO++;

      if ( !pFlowContext->pFlowModel->m_estimateParameters )
         {
		   if ( m_colWRShutOffMun != -1 )
		       {
			    if ( m_iduSWMunWRSOIndex[idu] == 1 )
				    m_pFlowModel->AddDelta( pFlowContext->pEnvContext, idu, m_colWRShutOffMun, 1 );

			    if ( m_iduSWMunWRSOIndex[idu] == 2 )
				    m_pFlowModel->AddDelta( pFlowContext->pEnvContext, idu, m_colWRShutOffMun, 2 );
		       }
         }

		// in acres
		m_iduSWAreaLevelOneIrrWRSO += m_iduSWLevelOneIrrWRSOAreaArray[idu] * ACRE_PER_M2;
		m_pouSWAreaLevelOneIrrWRSO += m_pouSWLevelOneIrrWRSOAreaArray[idu] * ACRE_PER_M2;
		m_iduSWAreaLevelTwoIrrWRSO += m_iduSWLevelTwoIrrWRSOAreaArray[idu] * ACRE_PER_M2;
		m_pouSWAreaLevelTwoIrrWRSO += m_pouSWLevelTwoIrrWRSOAreaArray[idu] * ACRE_PER_M2;

	   // m_iduIrrRequestArray is in units mm/day, conversion reference http://www.fao.org/docrep/x0490e/x0490e04.htm
		// ET mm/day = 10 m3/ha/day
		// AREA in IDU layer is assumed to be meters squared m2
		// availSourceFlow = m3/sec
		// 10000 m2/ha

		float totDyIrrmmDy = m_iduSWIrrArrayYr[idu] + m_iduGWIrrArrayYr[idu];
		float totDyIrrAcft = totDyIrrmmDy / 1000.0f * iduAreaM2 / M3_PER_ACREFT; // acre-feet
		float swDyIrrAcft = m_iduSWIrrArrayYr[idu] / 1000.0f * iduAreaM2 / M3_PER_ACREFT; // acre-feet
		float unsatIrrAcft =  m_iduUnsatIrrReqst_Yr[idu]; // acre-feet
		float swUnsatMunAcft =  m_iduSWUnsatMunDmdYr[idu] / 1000.0f * iduAreaM2 / M3_PER_ACREFT; // acre-feet
		float gwUnsatMunAcft =  m_iduGWUnsatMunDmdYr[idu] / 1000.0f * iduAreaM2 / M3_PER_ACREFT; // acre-feet
		float totUnsatMunAcft =  swUnsatMunAcft + gwUnsatMunAcft;
		float wastedIrrWaterAcft = m_iduWastedIrr_Yr[idu]; // acre-ft
		float excessIrrWaterAcft = m_iduExceededIrr_Yr[idu]; // acre-ft
		int   isMaxDutyExceeded = m_iduExceedDutyLog[idu]; // binary
		
		int daysInGrowingSeason  = ABS(harvDate) - plantDate; // defaults are harvdate = -999 and plantdate = 999

		if ( daysInGrowingSeason != 0 )
			m_aveTotDailyIrr[idu] = totDyIrrAcft / daysInGrowingSeason; // acre-feet

      m_pFlowModel->UpdateIDU( pFlowContext->pEnvContext, idu, m_colAllocatedIrrigation, totDyIrrmmDy, ADD_DELTA );
		m_pFlowModel->UpdateIDU( pFlowContext->pEnvContext, idu, m_colAllocatedIrrigationAf, totDyIrrAcft, ADD_DELTA );
		m_pFlowModel->UpdateIDU(pFlowContext->pEnvContext, idu, m_colUnsatIrrigationAf, unsatIrrAcft, ADD_DELTA );
		m_pFlowModel->UpdateIDU(pFlowContext->pEnvContext, idu, m_colSWUnsatIrrigationAf, 0, ADD_DELTA );
		m_pFlowModel->UpdateIDU(pFlowContext->pEnvContext, idu, m_colGWUnsatIrrigationAf, 0, ADD_DELTA );		 
		m_pFlowModel->UpdateIDU(pFlowContext->pEnvContext, idu, m_colSWAllocatedIrrigationAf, swDyIrrAcft, ADD_DELTA );
		m_pFlowModel->UpdateIDU(pFlowContext->pEnvContext, idu, m_colIrrRequestYr, m_iduIrrWaterRequestYr[idu], ADD_DELTA ) ;
		m_pFlowModel->UpdateIDU(pFlowContext->pEnvContext, idu, m_colMaxTotDailyIrr, m_maxTotDailyIrr[idu], ADD_DELTA ) ;
		m_pFlowModel->UpdateIDU(pFlowContext->pEnvContext, idu, m_colaveTotDailyIrr, m_aveTotDailyIrr[idu], ADD_DELTA ) ;
		m_pFlowModel->UpdateIDU(pFlowContext->pEnvContext, idu, m_colWastedIrrYr, wastedIrrWaterAcft, ADD_DELTA ) ;
		m_pFlowModel->UpdateIDU(pFlowContext->pEnvContext, idu, m_colExcessIrrYr, excessIrrWaterAcft, ADD_DELTA ) ;
		m_pFlowModel->UpdateIDU(pFlowContext->pEnvContext, idu, m_colIrrExceedMaxDutyYr, isMaxDutyExceeded, ADD_DELTA );

		if (m_iduGWIrrArrayYr[idu] > 0.0f)
			m_GWIrrAreaYr += iduAreaAc; // acre

		if (m_iduSWIrrArrayYr[idu] > 0.0f)
			m_SWIrrAreaYr += iduAreaAc; // acre

      int hru_id = -1; pLayer->GetData(idu, m_colHRU_ID, hru_id);
      HRU * pHRU = pFlowContext->pFlowModel->GetHRU(hru_id);
      aet_accumulator_m3 += (pHRU->m_et_yr / 1000.f) * iduAreaM2;
      float iduPRECIP_YR = pHRU->m_rainfall_yr + pHRU->m_snowfall_yr;
      m_pFlowModel->UpdateIDU(pFlowContext->pEnvContext, idu, m_colPRECIP_YR, iduPRECIP_YR, ADD_DELTA );
      precip_accumulator_m3 += (iduPRECIP_YR / 1000.f) * iduAreaM2;

      } // end IDU loop

   // Reset a few things associated with a WR, and test for Use it or lose it.
   for (int i = 0; i < (int)m_podArray.GetSize(); i++)
      {
      // Get current WR or POD
      WaterRight *pRight = m_podArray[i];

      vector<int> *pouIDUs = 0;
      /*
      if ( pRight->m_inConflict )
      {
      // count consequtive years a WR is in conflict (meaning WR is not being exercised)
      // economically unfeasible or non irrigation crop
      if (( pFlowContext->pEnvContext->currentYear - pRight->m_priorYearConflict) == 1 )
      {
      pRight->m_consecYearsNotUsed++;
      }
      else
      {
      pRight->m_consecYearsNotUsed = 0;
      }

      // use it, or lose it based on threshold set in .xml file
      if ( pRight->m_consecYearsNotUsed >= m_nYearsWRNotUsedLimit )
      {
      pRight->m_podStatus = WRPS_CANCELED;

      // Get POUID (aka Place of Use) for current Permit
      int pouID = pRight->m_pouID;

      // Build lookup key into point of use (POU) map/lookup table
      m_pouLookupKey.pouID = pouID;

      // Returns vector of indexs into the POU input data file. Used for relating to current water right POU to polygons in IDU layer
      pouIDUs = &m_pouInputMap[m_pouLookupKey];

      // if a PODID does not have a POUID, consider next WR
      if (pouIDUs->size() == 0)
      {
      continue;
      }

      float dailyDemand = 0.f;
      float POUarea = 0.f;

      m_idusPerPou = (int) pouIDUs->size();

      int irrPousPerPod = 0; // pous greater with over lap greater than user specified threshold
      int countConflict = 0;

      // Begin looping through IDUs Associated with WRID and PLace of Use (POU)
      for (int j = 0; j < pouIDUs->size(); j++)
      {
      int tempIDUNdx = m_pouDb.GetAsInt(m_colPouIDU_INDEX, pouIDUs->at(j));

      // Get the area of the POU for current WRid and IDU_INDEX
      float areaPou = m_pouDb.GetAsFloat(m_colPouArea, pouIDUs->at(j));

      //build key into idu index map
      m_iduIndexLookupKey.iduIndex = tempIDUNdx;

      //returns vector with the idu index for idu layer
      iduNdxVec = &m_IDUIndexMap[m_iduIndexLookupKey];

      //if no index is return, then idu the POU is associated with is not in current layer
      if (iduNdxVec->size() == 0)
      {
      continue;
      }
      else
      {
      iduNdx = iduNdxVec->at(0);
      }

      pLayer->GetData( iduNdx, m_colAREA, iduAreaM2 );


      if ( pRight->m_useCode == WRU_IRRIGATION )
      m_pFlowModel->UpdateIDU(pFlowContext->pEnvContext, idu ??? iduNdx, m_colIrrUseOrCancel, 1, ADD_DELTA );

      if ( pRight->m_useCode == WRU_MUNICIPAL )
      m_pFlowModel->UpdateIDU(pFlowContext->pEnvContext, idu ??? iduNdx, m_colMunUseOrCancel, 1, ADD_DELTA );

      } // for IDUs in POU getting canceled
      } // if canceling water right

      // update for next year
      pRight->m_priorYearConflict = pFlowContext->pEnvContext->currentYear;

      } //if pRight in conflict */

      // reset for next year
      pRight->m_inConflict = false;
      pRight->m_wrAnnualDuty = 0.0f;
      pRight->m_suspendForYear = false;
      pRight->m_stepsSuspended = 0; // this is reset if need be in a step, just here for last day of year
      } // for POD input table */
     
   float ugaGalPerPersonPerDay[MAX_UGA_NDX + 1];
   for (int uga = 1; uga <= MAX_UGA_NDX; uga++)
      {
      ugaGalPerPersonPerDay[uga] = ugaPop[uga] > 0.f ? (float)(((m_ugaUWallocatedYr[uga] / M3_PER_GAL) / ugaPop[uga]) / 365.f) : 0.f; // Convert m3/yr to gallons per capita per day.
      /*
         {
         CString msg;
         msg.Format("*** AltWaterMaster::EndYear() uga = %d, m_ugaUWallocatedYr[uga] = %f (m3), ugaPop[uga] = %f, ugaGalPerPersonPerDay[uga] = %f",
            uga, m_ugaUWallocatedYr[uga], (float)ugaPop[uga], ugaGalPerPersonPerDay[uga]);
         Report::Log(msg);
         }
      */
      if (m_ugaUWshortageYr[uga] > 0.f)
         {
         CString msg;
         msg.Format("*** AltWaterMaster::EndYear() UW shortage in UGA. uga = %d, m_ugaUWallocatedYr[uga] = %f (m3), ugaPop[uga] = %f, ugaGalPerPersonPerDay[uga] = %f, m_ugaUWshortageYr[uga] = %f (m3)",
            uga, m_ugaUWallocatedYr[uga], (float)ugaPop[uga], ugaGalPerPersonPerDay[uga], m_ugaUWshortageYr[uga]);
         Report::Log(msg);
         }
      } // end of loop on UGAs

   for (int idu = 0; idu < iduCount; idu++)
      {
      float gals_per_capita_per_day = 0.f;
      float iduUWfromSW_m3 = m_iduSWMuniArrayYr[idu];
      float iduUWfromGW_m3 = m_iduGWMuniArrayYr[idu];
      float iduUW_m3 = iduUWfromSW_m3 + iduUWfromGW_m3; // m3
      float iduPop = 0.f; pLayer->GetData(idu, m_colPOP, iduPop);
      int uga = -1; pLayer->GetData(idu, m_colUGB, uga);
      bool errFlag = false;
      if (uga >= 1 && uga <= MAX_UGA_NDX)
         { // Urban water use was accumulated by UGA.
         gals_per_capita_per_day = ugaGalPerPersonPerDay[uga];
         if (ugaPop[uga] > 0.f)
            {
            float pop_frac = (float)(iduPop / ugaPop[uga]);
            iduUWfromSW_m3 = m_ugaUWfromSW[uga] * pop_frac; // m3 H2O
            iduUWfromGW_m3 = m_ugaUWfromGW[uga] * pop_frac; // m3 H2O
            }
         else errFlag = (iduUW_m3 > 0.f);
         }
      else
         { // Urban water use was accumulated by IDU.
         if (iduUW_m3 > 0.f && iduPop > 0.f)
            {
            gals_per_capita_per_day = ((iduUW_m3 / M3_PER_GAL) / iduPop) / 365.f; // Convert from m3 to gals per capita per day.
            iduUWfromGW_m3 = m_iduGWMuniArrayYr[idu];
            }
         else errFlag = (iduUW_m3 > 0.f);
         }
      m_pFlowModel->UpdateIDU(pFlowContext->pEnvContext, idu, m_colGAL_CAP_DY, gals_per_capita_per_day, ADD_DELTA);
      m_pFlowModel->UpdateIDU(pFlowContext->pEnvContext, idu, m_colSWMUNALL_Y, iduUWfromSW_m3, ADD_DELTA);
      m_pFlowModel->UpdateIDU(pFlowContext->pEnvContext, idu, m_colGWMUNALL_Y, iduUWfromGW_m3, ADD_DELTA);

      float iduArea_m2 = 0.f; pLayer->GetData(idu, m_colAREA, iduArea_m2);
      float gwDyIrrAcft = ((m_iduGWIrrArrayYr[idu] / 1000.0f) * iduArea_m2) / M3_PER_ACREFT; // Convert mm to acre-feet
      m_pFlowModel->UpdateIDU(pFlowContext->pEnvContext, idu, m_colIRGWAF_Y, gwDyIrrAcft, ADD_DELTA);

      float iduGWMUNALL_Y_ac_ft = iduUWfromGW_m3 / M3_PER_ACREFT;
      groundwater_accumulator_ac_ft += gwDyIrrAcft + iduGWMUNALL_Y_ac_ft;

      if (errFlag)
         {
         CString msg; msg.Format("*** AltWaterMaster::EndYear() urban water use > 0 where pop <= 0. idu = %d, uga = %d, iduUW_m3 = %f, iduPop = %f", idu, uga, iduUW_m3, iduPop);
         Report::LogWarning(msg);
         gals_per_capita_per_day = 0.f;
         }
      } // end of IDU loop
        
   m_iduSWIrrWRSOLastWeek.Copy(m_iduSWIrrWRSOWeek);

	m_iduSWMunWRSOLastWeek.Copy(m_iduSWMunWRSOWeek);

	int time = pFlowContext->pEnvContext->currentYear;

	// summary for all irrigation
   CArray< float, float > rowAnnualMetrics;

   rowAnnualMetrics.SetSize(15);

   rowAnnualMetrics[0] = (float)time;

   //rowAnnualMetrics[1] = m_irrWaterRequestYr;
   rowAnnualMetrics[1] = m_irrigableIrrWaterRequestYr;
   rowAnnualMetrics[2] = m_irrigatedWaterYr;
   rowAnnualMetrics[3] = m_irrigatedSurfaceYr;
   rowAnnualMetrics[4] = m_irrigatedGroundYr;
   rowAnnualMetrics[5] = m_GWnoWR_Yr_m3 * ACREFT_PER_M3;
   rowAnnualMetrics[6] = 0.f;
   rowAnnualMetrics[7] = m_unSatisfiedIrrigationYr;
   rowAnnualMetrics[8] = m_wastedWaterVolYr;  // acre-ft/year
   rowAnnualMetrics[9] = m_exceededWaterVolYr;  // acre-ft/year
   rowAnnualMetrics[10] = m_areaDutyExceeds;  // acres
   rowAnnualMetrics[11] = m_demandDutyExceeds;  // acres-ft/year
   rowAnnualMetrics[12] = m_demandOutsideBegEndDates;  // acres-ft/year
   rowAnnualMetrics[13] = m_irrLenWtReachConflictYr; // meters
   rowAnnualMetrics[14] = m_unSatInstreamDemand; // m3/sec
   this->m_annualMetrics.AppendRow(rowAnnualMetrics);

   float precip_yr_mm = (float)((precip_accumulator_m3 / tot_area) * 1000.f);
   float aet_yr_mm = (float)((aet_accumulator_m3 / tot_area) * 1000.f);
   float groundwater_pumped_mm = (float)((groundwater_accumulator_ac_ft * M3_PER_ACREFT) / tot_area) * 1000.f;
   float from_outside_basin_mm = (float)((m_fromOutsideBasinYr / tot_area) * 1000.f);
   // <output in_use = "1" name = "WRB-Discharge (m3/s)" query = "COMID=23735691" value = "reachOutflow" type = "sum" domain = "reach" / > 
   float basin_specific_discharge = (float)(m_basin_discharge_accumulator_m3 / tot_area);
   float tot_input_mm = precip_yr_mm + groundwater_pumped_mm + from_outside_basin_mm;
   float tot_output_mm = aet_yr_mm + basin_specific_discharge * 1000.f;
   float water_balance_residual_fraction = (tot_input_mm - tot_output_mm) / tot_input_mm;
   CArray< float, float > rowQuickCheckMetrics;
   rowQuickCheckMetrics.SetSize(7);
   rowQuickCheckMetrics[0] = (float)time;
   rowQuickCheckMetrics[1] = precip_yr_mm;
   rowQuickCheckMetrics[2] = groundwater_pumped_mm;
   rowQuickCheckMetrics[3] = from_outside_basin_mm;
   rowQuickCheckMetrics[4] = aet_yr_mm;
   rowQuickCheckMetrics[5] = basin_specific_discharge;
   rowQuickCheckMetrics[6] = water_balance_residual_fraction;
   this->m_QuickCheckMetrics.AppendRow(rowQuickCheckMetrics);
/*
	if ( m_debug )
		{
		// summary for all irrigation
		CArray< float, float > rowAnnualMetricsDebug;

		rowAnnualMetricsDebug.SetSize(12);
		rowAnnualMetricsDebug[0] = (float) time; // "Year"
		rowAnnualMetricsDebug[1] = m_anGTmaxDutyArea21;
		rowAnnualMetricsDebug[2] = m_anGTmaxDutyArea22;
		rowAnnualMetricsDebug[3] = m_anGTmaxDutyArea23;
		rowAnnualMetricsDebug[4] = m_anGTmaxDutyArea24;
		rowAnnualMetricsDebug[5] = m_anGTmaxDutyArea25;
		rowAnnualMetricsDebug[6] = m_anGTmaxDutyArea26;
		rowAnnualMetricsDebug[7] = m_anGTmaxDutyArea27;
		rowAnnualMetricsDebug[8] = m_anGTmaxDutyArea28;
		rowAnnualMetricsDebug[9] = m_anGTmaxDutyArea29;
		rowAnnualMetricsDebug[10]= (float) m_pastureIDUGTmaxDuty;
		rowAnnualMetricsDebug[11]= m_pastureIDUGTmaxDutyArea;
		this->m_annualMetricsDebug.AppendRow(rowAnnualMetricsDebug);

		//reset for next year
		m_anGTmaxDutyArea21 = 0.0f;
		m_anGTmaxDutyArea22 = 0.0f;
		m_anGTmaxDutyArea23 = 0.0f;
		m_anGTmaxDutyArea24 = 0.0f;
		m_anGTmaxDutyArea25 = 0.0f;
		m_anGTmaxDutyArea26 = 0.0f;
		m_anGTmaxDutyArea27 = 0.0f;
		m_anGTmaxDutyArea28 = 0.0f;
		m_anGTmaxDutyArea29 = 0.0f;
		}
*/
   m_daysPerYrMaxRateExceeded /= m_SWIrrAreaYr;  // apply normalization.  Note, we accumulate these as iduValue * iduArea, over a year

  	return true;
   } // end of EndYear()


int AltWaterMaster::IDUatXY(double xCoord, double yCoord, int uga, MapLayer *pLayer)
   {
   float least_distance = 1.e9f; // initialize least_distance to a million km
   int best_idu = -1;
   for (MapLayer::Iterator idu = pLayer->Begin(); idu != pLayer->End(); idu++)
      {
      int iduUGA = -1; pLayer->GetData(idu, m_colUGB, iduUGA);
      if (iduUGA != uga) continue;

      Poly *pPoly = pLayer->GetPolygon(idu);
      Vertex idu_centroid = pPoly->GetCentroid();
      if (idu_centroid.x != idu_centroid.x || idu_centroid.y != idu_centroid.y) continue; // detect and skip NaNs
      float dx = float(idu_centroid.x - xCoord);
      float dy = float(idu_centroid.y - yCoord);
      float distance = sqrt(dx*dx + dy*dy);
      if (distance < least_distance) best_idu = idu;

      } // end of loop thru IDUs

   return(best_idu);
   } // end of IDUatXY()
   

float AltWaterMaster::GetAvailableSourceFlow(Reach *pReach)
	{
	// this method calculates the amount of demand that can be extracted from this stream reach

	float flow = (pReach->GetDischarge()); // m3/day

	float instreamWRUse = pReach->m_instreamWaterRightUse;

	if (flow > m_minFlow)
		return flow - m_minFlow - instreamWRUse;
	else
		return m_minFlow;
	}

int AltWaterMaster::LoadWRDatabase(FlowContext *pFlowContext)
	{

	MapLayer *pStreamLayer = (MapLayer*)pFlowContext->pFlowModel->m_pStreamLayer;

	MapLayer *pLayer = (MapLayer*)pFlowContext->pEnvContext->pMapLayer;

   for (int uga = 0; uga <= MAX_UGA_NDX; uga++) m_ugaDischargeComID[uga] = 0;


   // *************** Begin loading Point of Diversion input file ****************************************

	int podRecords = m_podDb.ReadAscii(m_podTablePath); // Pre-sorted data file. sorted by PriorityYear, PriorityMonth, PriorityDOY, and BeginDOY respectively

	if (podRecords == 0)
		{
		CString msg;
		msg.Format("AltWM::LoadWRDatabase could not load Point of Diversion .csv file \n");
		Report::InfoMsg(msg);
		return 0;
		}

   // http://www.oregon.gov/owrd/pages/wr/wrisuse.aspx
	m_colWRID = m_podDb.GetCol("WATERRIGHTID");
	m_colXcoord = m_podDb.GetCol("x");
	m_colYcoord = m_podDb.GetCol("y");
	m_colPodID = m_podDb.GetCol("PODID");
	m_colPDPouID = m_podDb.GetCol("POUID");
	m_colPermitCode = m_podDb.GetCol("PERMITCODE");
	m_colPodRate = m_podDb.GetCol("PODRATE");
	m_colUseCode = m_podDb.GetCol("USECODE");
	m_colPriorDoy = m_podDb.GetCol("PRIORITYDOY");
	m_colPriorYr = m_podDb.GetCol("YEAR");
	m_colBeginDoy = m_podDb.GetCol("BEGINDOY");
	m_colEndDoy = m_podDb.GetCol("ENDDOY");
	m_colReachComid = m_podDb.GetCol("REACHCOMID");
	m_colPodUseRate = m_podDb.GetCol("PODUSERATE");
	m_colDistPodReach = m_podDb.GetCol("DISTANCE");
	m_colMaxDutyFile = m_podDb.GetCol(m_maxDutyColName); // may or may not exist

	if ( m_colXcoord   < 0 || m_colPermitCode < 0 || m_colPriorDoy < 0 || m_colWRID        < 0 ||
		  m_colYcoord   < 0 || m_colPodRate    < 0 || m_colPriorYr  < 0 || m_colPDPouID     < 0 ||
		  m_colPodID    < 0 || m_colUseCode    < 0 || m_colBeginDoy < 0 || /* DRC m_colPodStatus   < 0 || */
		m_colPodRate     < 0 || m_colPodUseRate < 0 || m_colDistPodReach < 0)
		{
		CString msg;
		msg.Format("AltWM::LoadWRDatabase: One or more column headings are incorrect in Point of Diversion data input file\n");
		Report::ErrorMsg(msg);
      msg.Format("m_podTablePath = %s\n"
         "m_colXcoord = %d, m_colPermitCode = %d, m_colPriorDoy = %d, m_colWRID = %d\n"
         "m_colYcoord = %d, m_colPodRate = %d, m_colPriorYr = %d, m_colPDPouID = %d\n"
         "m_colPodID = %d, m_colUseCode = %d, m_colBeginDoy = %d, m_colEndDoy = %d\n"
         "m_colPodRate = %d, m_colPodUseRate = %d, m_colDistPodReach = %d",
         m_podTablePath,
         m_colXcoord, m_colPermitCode, m_colPriorDoy, m_colWRID,
         m_colYcoord, m_colPodRate, m_colPriorYr, m_colPDPouID,
         m_colPodID, m_colUseCode, m_colBeginDoy, m_colEndDoy,
         m_colPodRate, m_colPodUseRate, m_colDistPodReach);
      Report::InfoMsg(msg);
		return 0;
		}

	m_mostJuniorWR = 4000;
	m_mostSeniorWR = 0;
   int notFound_count = 0;
	for (int i = 0; i < podRecords; i++)
		{
      int wrID = m_podDb.GetAsInt(m_colWRID, i);
      if (wrID < 0) continue; // Skip over placeholder entries in the POD file.

      WaterRight *pRight = new WaterRight;

		// set up water right from table
		pRight->m_wrID = wrID;
		pRight->m_xCoord = m_podDb.GetAsDouble(m_colXcoord, i);
		pRight->m_yCoord = m_podDb.GetAsDouble(m_colYcoord, i);
		pRight->m_podID = m_podDb.GetAsInt(m_colPodID, i);
		pRight->m_pouID = m_podDb.GetAsInt(m_colPDPouID, i);
		pRight->m_appCode = -99; // m_podDb.GetAsInt(m_colAppCode, i); DRC
		pRight->m_permitCode = (WR_PERMIT)m_podDb.GetAsInt(m_colPermitCode, i);
		pRight->m_podRate = m_podDb.GetAsFloat(m_colPodRate, i);
		pRight->m_useCode = (WR_USE)m_podDb.GetAsInt(m_colUseCode, i);
		pRight->m_supp = -99; // m_podDb.GetAsInt(m_colSupp, i); DRC
		pRight->m_priorDoy = m_podDb.GetAsInt(m_colPriorDoy, i);
		pRight->m_priorYr = m_podDb.GetAsInt(m_colPriorYr, i);
		pRight->m_beginDoy = m_podDb.GetAsInt(m_colBeginDoy, i);
		pRight->m_endDoy = m_podDb.GetAsInt(m_colEndDoy, i);
      if (pRight->m_useCode == WRU_IRRIGATION)
         {
         pRight->m_beginDoy = m_irrDefaultBeginDoy; // 60; // March 1st, where Jan 1 = 1
         pRight->m_endDoy = m_irrDefaultEndDoy; // 304; // Oct 31st, where Jan 1 = 1 and no leap year
         }
		pRight->m_reachComid = m_podDb.GetAsInt(m_colReachComid, i);
		pRight->m_podUseRate = m_podDb.GetAsFloat(m_colPodUseRate, i);
		pRight->m_distanceToReach = m_podDb.GetAsFloat(m_colDistPodReach, i);
      pRight->m_podStatus = WRPS_NONCANCELED; // We assume that cancelled water rights are not present in the POD csv file.

		if ( pRight->m_priorYr < m_mostJuniorWR ) m_mostJuniorWR = pRight->m_priorYr;
		if ( pRight->m_priorYr > m_mostSeniorWR ) m_mostSeniorWR = pRight->m_priorYr;

      if (pRight->m_pouID < 0)
         { // The "POU" for this WR is a UGA.
         int uga = -pRight->m_pouID;
         if (pRight->m_reachComid == -99)
            { // This POD is outside the Willamette River basin.
            CString msg;
            msg.Format("*** LoadWRDatabase(): uga %d gets water from outside the basin. pRight->m_wrID = %d, pRight->m_podID = %d", uga, pRight->m_wrID, pRight->m_podID);
            Report::Log(msg);
            }

         int idu = IDUatXY(pRight->m_xCoord, pRight->m_yCoord, uga, pLayer);
         if (idu >= 0)
            {
            int ComID; pLayer->GetData(idu, m_coliDULayComid, ComID);
            if (pRight->m_reachComid == 0) pRight->m_reachComid = ComID; // If the reachComid hasn't been filled in, do it now.
            if (m_ugaDischargeComID[uga] <= 0)
               {
               m_ugaDischargeComID[uga] = ComID;
               CString msg;
               msg.Format("*** LoadWRDatabase(): Setting the discharge for uga %d in reach ComID = %d, idu = %d", uga, ComID, idu);
               Report::Log(msg);
               }
            }
         } // end of POU-is-UGA logic
      else if (pRight->m_reachComid == -99)
         { // This POD is outside the Willamette River basin.
         CString msg;
         msg.Format("*** LoadWRDatabase(): This point of diversion is outside the basin. pRight->m_wrID = %d, pRight->m_podID = %d", pRight->m_wrID, pRight->m_podID);
         Report::Log(msg);
         }

		// if this exist in POD input file.  If also exist in IDU layer, IDU layer value is used.
		// If neither exist in IDU layer or POD file, then the default duty set in .xml is used.
		if ( m_colMaxDutyFile != -1 ) 
			pRight->m_maxDutyPOD = m_podDb.GetAsFloat(m_colMaxDutyFile, i);

		int reachID = pRight->m_reachComid;     // unique reach identifier stored in WR table, use COMID

		if (i < (podRecords - 1))
			{
			int nextWRID = m_podDb.GetAsInt(m_colWRID, i + 1);
			int nextPODID = m_podDb.GetAsInt(m_colPodID, i + 1);
			int nextPOUID = m_podDb.GetAsInt(m_colPDPouID, i + 1);
			int nextUse = m_podDb.GetAsInt(m_colUseCode, i + 1);

			// count how many PODs per Water Right, per POU, per use. The default is 1
			if (pRight->m_wrID == nextWRID &&
				pRight->m_pouID == nextPOUID &&
				pRight->m_useCode == nextUse &&
				pRight->m_podID != nextPODID)
				{
				pRight->m_nPODSperWR++;
				}
			}

		// find corresponding reach.  Note that reachID = -99 is Point of Diversion out of Basin and not being modeled
      bool foundReach_flag = false;
      pRight->m_pReach = NULL;
      if (reachID == -99 || reachID == 0) foundReach_flag = true;
      else if ( reachID > 0 )
			{
			int index = pStreamLayer->FindIndex(m_colStrLayComid, reachID, 0);			
         if (index < 0) notFound_count++; 
			else
				{ // found index, now find reach
			   pRight->m_pReach = pFlowContext->pFlowModel->FindReachFromIndex(index);
            foundReach_flag = pRight->m_pReach != NULL;
            }
			}
		
		// If we found the reach, add to our array.
		if (foundReach_flag) m_podArray.Add(pRight);

      // begin DRC - Check that POD file is in sorted order.
      if (i > 0)
         {
         int yr, yrPrev, doy, doyPrev, begin_doy, begin_doyPrev, end_doy, end_doyPrev;
         yr = m_podArray[i]->m_priorYr; yrPrev = m_podArray[i - 1]->m_priorYr;
         doy = m_podArray[i]->m_priorDoy ; doyPrev = m_podArray[i - 1]->m_priorDoy;
         begin_doy = m_podArray[i]->m_beginDoy ; begin_doyPrev = m_podArray[i - 1]->m_beginDoy;
         end_doy = m_podArray[i]->m_endDoy; end_doyPrev = m_podArray[i - 1]->m_endDoy;
         if (!(yr > yrPrev
            || (yr == yrPrev && doy >= doyPrev)))
            {
            CString msg;
            msg.Format("AltWM::LoadWRDatabase: PODs out of order in input file %s at record # %d. yr = %d, yrPrev = %d, doy = %d, doyPrev = %d, "
               "begin_doy = %d, begin_doyPrev=%d, end_doy = %d, end_doyPrev = %d", 
               m_podTablePath, i, yr, yrPrev, doy, doyPrev, begin_doy, begin_doyPrev, end_doy, end_doyPrev);
            Report::LogWarning(msg);
            }
         }
      // end DRC - Check that POD file is in sorted order. */

		} // end of loop thru the records in the POD data file

   if (notFound_count > 0)
      {
      CString msg;
      msg.Format("AltWaterMaster::LoadWRDatabase() There were %d records in %s for which the reach ComID was not found in the stream layer.", 
         notFound_count, m_podTablePath);
      Report::Log(msg);
      }



	// *************** End loading Point of Diversion input file ****************************************

	// *************** Begin loading Place of Use input file ****************************************

	int pouDBNdx;

	int pouRecords = m_pouDb.ReadAscii(m_pouTablePath); // Point of Use (POU) data input file

	if (pouRecords == 0)
		{
		CString msg;
		msg.Format("AltWM::LoadWRDatabase could not load Point of Use .csv file \n");
		Report::InfoMsg(msg);
		return 0;
		}

	m_colPUPouID = m_pouDb.GetCol("POUID");        // WR POU ID column number in POU input data file
	m_colPouIDU_INDEX = m_pouDb.GetCol("IDU_INDEX");    // Relates to the IDU_INDEX attribute in the IDU layer
	m_colPouPct = m_pouDb.GetCol("PERCENT");      // The areal percentage of the POU, for a POUID, that over laps the IDU/IDU_INDEX
	m_colPouDBNdx = m_pouDb.GetCol("POU_INDEX");    // a zero based index for the POU input data file itself
	m_colPouArea = m_pouDb.GetCol("AREA");         // The area of the POU, for a SnapID, that over laps the IDU/IDU
	m_colPouUSECODE = m_pouDb.GetCol("USECODE");
	m_colPouPERMITCODE = m_pouDb.GetCol("PERMITCODE");

  if (m_colPUPouID   < 0 || m_colPouIDU_INDEX < 0 || m_colPouPct < 0 || m_colPouDBNdx < 0 || 
      m_colPouArea < 0 || m_colPouUSECODE < 0 || m_colPouPERMITCODE < 0)
	{
	CString msg;
	msg.Format("AltWM::LoadWRDatabase: One or more column headings are incorrect in Point of Use data input file\n");
	Report::ErrorMsg(msg);
	return 0;
	}

   for (int i = 0; i < pouRecords; i++)
      {

      //Build the Key for the map lookup
      m_pouDb.Get(m_colPUPouID, i, m_pouInsertKey.pouID);

      //Result from the Map lookup
      m_pouDb.Get(m_colPouDBNdx, i, pouDBNdx);

      //Build the Map
      m_pouInputMap[m_pouInsertKey].push_back(pouDBNdx);

      }
      // *************** End loading Place of Use input file ****************************************

   int nIDUs = IDUIndexLookupMap(pFlowContext); // Create an IDU index map, used for sub basins and partial IDU layer loading.
      {
      CString msg;
      msg.Format("AltWM::LoadWRDatabase() nIDUs = %d", nIDUs);
      Report::Log(msg);
      }
   CalculateWRattributes(pLayer, pouRecords);

	return podRecords;
   } // end of LoadWRDatabase()


   void AltWaterMaster::CalculateWRattributes(MapLayer *pLayer, int pouRecords)
   {  // Calculate WREXISTS, WR_MUNI, WR_INSTRM, WR_IRRIG_S, WR_IRRIG_G and STRM_ORDER.
   CString msg;
   msg.Format("AltWM::CalculateWRattributes() starting now. pouRecords = %d", pouRecords);
   Report::Log(msg);

   bool readOnlyFlag = pLayer->m_readOnly;
   pLayer->m_readOnly = false;
   pLayer->SetColData(m_colWREXISTS, VData(0), true);
   pLayer->SetColData(m_colWR_MUNI, VData(0), true);
   pLayer->SetColData(m_colWR_INSTRM, VData(0), true);
   pLayer->SetColData(m_colWR_IRRIG_S, VData(0), true);
   pLayer->SetColData(m_colWR_IRRIG_G, VData(0), true);
   pLayer->SetColData(m_colSTRM_ORDER, VData(0), true);

   for (int i = 0; i < pouRecords; i++)
      {
      // If the IDU overlap fraction is less than the threshold value, ignore this record. (DRC addition starts here.)
      float threshold_value = 0.02f;
      int idu_index = -1; m_pouDb.Get(m_colPouIDU_INDEX, i, idu_index);

      m_pouDb.Get(m_colPouIDU_INDEX, i, m_iduIndexLookupKey.iduIndex); // Get the idu_index as it exists in the database for the whole study area.
      vector<int> *iduNdxVec = 0;
      iduNdxVec = &m_IDUIndexMap[m_iduIndexLookupKey]; // Returns a vector of length 0 or consisting of a single local idu index.
      if (iduNdxVec->size() == 0) continue; // If iduNdxVec has length 0, then the IDU referenced in this record is not in in the current layer.
      int idu = iduNdxVec->at(0); // idu is the local index of the idu of interest in the idu database, whether the database is for the whole study area or a subbasin.
/*       {
         CString msg;
         msg.Format("*** CalculateWRattributes(): i = %d, m_iduIndexLookupKey.iduIndex = %d, iduNdxVec->size() = %d, idu = %d", i, m_iduIndexLookupKey.iduIndex, iduNdxVec->size(), idu);
         Report::Log(msg);
         }
*/
      float idu_area = -1.f; pLayer->GetData(idu, m_colAREA, idu_area);
      float overlap_area = -1.f; m_pouDb.Get(m_colPouArea, i, overlap_area);
      if (overlap_area / idu_area < threshold_value) continue;

      int use_code = 0;
      m_pouDb.Get(m_colPouUSECODE, i, use_code);
      int permit_code = 0;
      m_pouDb.Get(m_colPouPERMITCODE, i, permit_code);
      int wr_exists = 0;
      pLayer->GetData(idu, m_colWREXISTS, wr_exists);

      wr_exists |= permit_code;
      wr_exists |= (use_code * 256);

      bool readOnlyFlag = pLayer->m_readOnly;
      pLayer->m_readOnly = false;

      pLayer->SetData(idu, m_colWREXISTS, wr_exists);

      CString msg;
      switch (use_code)
         {
         case (int)WRU_IRRIGATION:
            if (permit_code == (int)WRP_SURFACE) pLayer->SetData(idu, m_colWR_IRRIG_S, 1);
            else if (permit_code == (int)WRP_GROUNDWATER) pLayer->SetData(idu, m_colWR_IRRIG_G, 1);
            break;
         case (int)WRU_MUNICIPAL: pLayer->SetData(idu, m_colWR_MUNI, 1); break;
         case (int)WRU_INSTREAM: pLayer->SetData(idu, m_colWR_INSTRM, 1); break;
         default:
            //msg.Format("WaterMaster::LoadWRDatabase() USE_CODE in POU file is not WRU_IRRIGATION, WRU_MUNICIPAL, or WRU_INSTREAM. use_code = %d, idu_index = %d", 
            //use_code, idu);
            //Report::Log(msg);
            break;
         } // end of switch(use_code)
      } // end of loop thru POUs

   pLayer->m_readOnly = readOnlyFlag;

   msg.Format("AltWM::CalculateWRattributes() is completing now.");
   Report::Log(msg);

   } // end CalculateWRattributes()


int AltWaterMaster::LoadDynamicWRDatabase(FlowContext *pFlowContext)
	{
	MapLayer *pStreamLayer = (MapLayer*)pFlowContext->pFlowModel->m_pStreamLayer;

	MapLayer *pLayer = (MapLayer*)pFlowContext->pEnvContext->pMapLayer;	
			
	CString colLabel;

	int dynamWRRecords = m_dynamicWRDb.ReadAscii( m_dynamicWRTablePath ); 

	if (dynamWRRecords == 0)
		{
		CString msg;
		msg.Format("AltWM::LoadDynamicWRDatabase could not load dynamic WR .csv file (specified in .xml file) \n");
		Report::InfoMsg(msg);
		return false;
		}

	int colCount = m_dynamicWRDb.GetColCount();

	int nWRZones = 0;

	for ( int c = 0; c < colCount; c++ )
		{
		 colLabel = m_dynamicWRDb.GetLabel(c);

		 int foundZone = colLabel.Find( "WRZONE", 0 );

		 if ( foundZone >= 0 )  
			{
			nWRZones++;
			m_zoneLabels.Add( colLabel );
			}
		}
	
	if ( nWRZones == 0 )
		{
		CString msg;
		msg.Format("AltWM::LoadDynamicWRDatabase: No Water right zones (WRZONE1..WRZONEn) specified in DynamicWaterRights .csv file (should match IDU Layer) \n");
		Report::ErrorMsg(msg);
		return false;
		}

	// these columns must exist, similar to other input files etc...
	m_colDynamTimeStep	= m_dynamicWRDb.GetCol("TIMESTEP");
	m_colDynamPermitCode = m_dynamicWRDb.GetCol("PERMITCODE");
	m_colDynamUseCode	   = m_dynamicWRDb.GetCol("USECODE");
	m_colDynamIsLease	   = m_dynamicWRDb.GetCol("ISLEASE");
	
     int flag = (m_colDynamTimeStep < 0 ? 1 : 0 );
     flag += ( m_colDynamPermitCode < 0 ? 2 : 0 );
     flag += ( m_colDynamUseCode    < 0 ? 4 : 0 );
     flag += ( m_colDynamIsLease    < 0 ? 8 : 0 );

	if ( flag != 0  )
        {
		CString msg;
		msg.Format("AltWM::LoadDynamicWRDatabase: One or more column headings are incorrect in DynamicWaterRights .csv file.  Error code:%i\n", flag );
		Report::ErrorMsg(msg);
		return false;
		}
	
	pLayer->CheckCol( m_colWRZone, "WRZONE" , TYPE_LONG, CC_MUST_EXIST );

	  if ( m_colWRZone < 0  )
		{
		CString msg;
		msg.Format("AltWM::LoadDynamicWRDatabase: WRZONE(s) found in DynamicWaterRights .csv file, WRZONE must exist in IDU layer\n");
		Report::ErrorMsg(msg);
		return false;
		}

	if ( nWRZones > 0 && m_colWRZone != -1 ) m_DynamicWRs = true;
		
	return dynamWRRecords;
	}

int AltWaterMaster::IDUIndexLookupMap(FlowContext *pFlowContext)
   {
	int nIDUs = -1;

	MapLayer *pLayer = (MapLayer*)pFlowContext->pEnvContext->pMapLayer;

	for (MapLayer::Iterator idu = pLayer->Begin(); idu != pLayer->End(); idu++)
	   {
		// Build the key for the map lookup
		pLayer->GetData(idu, m_colIDUIndex, m_iduIndexInsertKey.iduIndex);

		// Build the map
		m_IDUIndexMap[m_iduIndexInsertKey].push_back(idu);

		nIDUs = idu;
	   }

	return nIDUs;
   }

int AltWaterMaster::SortWrData(FlowContext *pFlowContext, PtrArray<WaterRight> arrayIn)
	{
	return 1;
	}
	
bool AltWaterMaster::DynamicWaterRights( FlowContext *pFlowContext, int scenarioValue, int interval )
	{
	MapLayer *pLayer = (MapLayer*)pFlowContext->pEnvContext->pMapLayer;	

	int currentStep = pFlowContext->pEnvContext->yearOfRun;
	
	bool ranExtremeRes = false;
	bool ranZoneTargets = false;
	int dyWRInterval = interval; 
	int records = 0;

   switch ( scenarioValue )
      {

      case 1:
		   
			// Run only in InitRun
			if ( currentStep == -1 )
				ranExtremeRes = ExtremeResWaterRights( pFlowContext, m_dynamicWRRadius );

         break;

      case 2:
			
			// load only in first step
			if (currentStep == 0)
				{
				pLayer->CheckCol( m_colWRZone, "WRZONE" , TYPE_LONG, CC_MUST_EXIST );
				records = LoadDynamicWRDatabase( pFlowContext );
				}

         ranZoneTargets = ZoneTargetResWaterRights( pFlowContext, currentStep, dyWRInterval, m_dynamicWRRadius );

         break;

      default:
         
			//runs just the input POD and POU input files specified in .xml
         break;

      }

	return true;
	}

bool AltWaterMaster::ZoneTargetResWaterRights(FlowContext *pFlowContext, int currentStep, int stepInterval, float radius)
   {
   MapLayer *pLayer = (MapLayer*)pFlowContext->pEnvContext->pMapLayer;

   MapLayer* pStreamLayer = (MapLayer*)pFlowContext->pFlowModel->m_pStreamLayer;

   // The number of zones specified in the dynamic WR input file.  These names should relate to attributes in IDU layer
   int zoneCount = (int)m_zoneLabels.GetCount();

   CArray<float, float>  zoneTotals;
   CArray<int, int>		zoneNumbers;
   CArray<int, int> *dsReaches = new CArray < int, int > ;

   int dsReservoir = 0;

   int reachCount = pFlowContext->pFlowModel->GetReachCount();

   // if this method is run in InitRun
   if (currentStep == -1)
      currentStep = 0;

   // get array of reaches below reservoirs
   for (int i = 0; i < reachCount; i++)
      {

      int dsReservoir = 0;

      pStreamLayer->GetData(i, m_colDSReservoir, dsReservoir);

      if (dsReservoir == 1)
         {
         int streamLayerComid = -1;
         int iduLayerComid = -1;

         pStreamLayer->GetData(i, m_colStrLayComid, streamLayerComid);
         dsReaches->Add(streamLayerComid);
         }
      }

   for (int z = 0; z < zoneCount; z++)
      {
      float zoneTotal = 0.0f;
      int iduColZone = -1;

      CString zoneLabel = m_zoneLabels[z];

      int zoneNumber = atoi(zoneLabel.Mid(6)); // should relate to value in IDU attribute WRZONE

      int dbZoneColumn = m_dynamicWRDb.GetCol((LPCTSTR)zoneLabel); // example column in database file is WRZONE1

      for (int r = currentStep; r <= currentStep + stepInterval; r++)
         {

         float dynamWRrequest = m_dynamicWRDb.GetAsFloat(dbZoneColumn, r);
         zoneTotal += dynamWRrequest; // acre-ft

         } // endfor dynamicWR records 

      zoneTotals.Add(zoneTotal);// acre-ft
      zoneNumbers.Add(zoneNumber);
      vector< int >  zoneIDUIndexs;

      int iduNdx = 0;

      for (MapLayer::Iterator idu = pLayer->Begin(); idu != pLayer->End(); idu++)
         {
         int iduIndex = -1;
         int iduZone = -1;
         int wrexist = 0;
         int lulcA = -1;

         ASSERT(m_colWRZone >= 0);
         pLayer->GetData(idu, m_colWRZone, iduZone);
         pLayer->GetData(idu, m_colWREXISTS, wrexist);
         pLayer->GetData(idu, m_colLulc_A, lulcA);

         // for the current zone, no water rights exist and agriculture
         if (iduZone == zoneNumber && wrexist == 0 && lulcA == 2)
            {
            zoneIDUIndexs.push_back(iduNdx);
            }
         iduNdx++;
         } // endfor IDU 

      if (zoneIDUIndexs.size() != 0)
         {
         random_shuffle(zoneIDUIndexs.begin(), zoneIDUIndexs.end());
         }
      else
         {
         continue;
         }

      float sumDemandAcft = 0.0;

      int index = 0;

      int count = (int)zoneIDUIndexs.size();

      while (sumDemandAcft < zoneTotal && index < count)
         {
         float iduAreaM2 = 0.0;
         int destinationIDU = zoneIDUIndexs.at(index);

         pLayer->GetData(destinationIDU, m_colAREA, iduAreaM2);

         float iduAreaAc = iduAreaM2 * ACRE_PER_M2;

         // m_maxRate is in cfs/acre and specified in .xml file
         float DemandCfs = m_maxRate * iduAreaAc; // cfs
         sumDemandAcft += (float)DemandCfs * 4.73518839f; // acre-ft

         int streamLayerComid = GetNearestReach(pFlowContext, destinationIDU, dsReaches, m_dynamicWRRadius);

         if (streamLayerComid != -1)
            AddWaterRight(pFlowContext, destinationIDU, streamLayerComid);

         index++;
         }
      } // endfor WRZONE

   if (m_debug)
      {
      OutputUpdatedWRLists();
      }

   return true;
   } // end of ZoneTargetResWaterRights()


   bool AltWaterMaster::ExtremeResWaterRights(FlowContext *pFlowContext, float radius)
      {
      MapLayer *pLayer = (MapLayer*)pFlowContext->pEnvContext->pMapLayer;

      MapLayer* pStreamLayer = (MapLayer*)pFlowContext->pFlowModel->m_pStreamLayer;

      pLayer->CheckCol(m_colWRZone, "WRZONE", TYPE_LONG, CC_MUST_EXIST);

      int colAddExResWR = -1;

      if (m_debug)
         pLayer->CheckCol(colAddExResWR, "ADDEXRESWR", TYPE_INT, CC_AUTOADD);

      pLayer->CheckCol(m_colWRShortG, "WR_SHORTG", TYPE_INT, CC_AUTOADD);

      ASSERT(m_colWRZone >= 0);

      CArray<int, int> dsIduIndexs;

      int dsReservoir = 0;

      int reachCount = pFlowContext->pFlowModel->GetReachCount();

      CArray<int, int> haveSeen;

      for (int i = 0; i < reachCount; i++)
         {

         int dsReservoir = 0;

         pStreamLayer->GetData(i, m_colDSReservoir, dsReservoir);

         Poly *iduPoly;

         if (dsReservoir == 1)
            {
            int streamLayerComid = -1;
            int iduLayerComid = -1;

            pStreamLayer->GetData(i, m_colStrLayComid, streamLayerComid);

            for (MapLayer::Iterator idu = pLayer->Begin(); idu != pLayer->End(); idu++)
               {
               pLayer->GetData(idu, m_coliDULayComid, iduLayerComid);
               if (streamLayerComid == iduLayerComid)
                  {
                  iduPoly = pLayer->GetPolygon(idu);
                  break;
                  }
               }

            const int maxNeighbors = 2048;

            int neighbors[maxNeighbors];

            int count = pLayer->GetNearbyPolys(iduPoly, neighbors, NULL, maxNeighbors, radius);

            for (int p = 0; p < count; p++)
               {
               int iduIndex = -1;
               int iduZone = 1;
               int wrexist = 0;
               int lulcA = -1;

               pLayer->GetData(neighbors[p], m_colIDUIndex, iduIndex);
               pLayer->GetData(neighbors[p], m_colWRZone, iduZone);
               pLayer->GetData(neighbors[p], m_colWREXISTS, wrexist);
               pLayer->GetData(neighbors[p], m_colLulc_A, lulcA);

               if (wrexist == 0 && lulcA == 2)
                  {

                  bool haveSeenIDU = false;

                  for (int h = 0; h < haveSeen.GetSize(); h++)
                     {
                     if (haveSeen[h] == neighbors[p])
                        {
                        haveSeenIDU = true;
                        break;
                        }
                     else
                        {
                        haveSeen.Add(neighbors[p]);
                        break;
                        }
                     }

                  if (!haveSeenIDU)
                     {

                     AddWaterRight(pFlowContext, neighbors[p], streamLayerComid); // Action Item 1984 needs xml input variable

                     if (m_debug)
                        m_pFlowModel->UpdateIDU(pFlowContext->pEnvContext, neighbors[p], colAddExResWR, 1, ADD_DELTA);

                     } // endif have not seen this IDU				
                  } // endif no water right exist and is ag
               } // endfor nearest polys to reservoir reach
            } // endif reservoir reach
         } // endfor reach	

      if (m_debug)
         {
         OutputUpdatedWRLists();
         }

      return true;
      }


int AltWaterMaster:: GetNearestReach(FlowContext *pFlowContext, int idu, CArray<int, int> *reaches, float radius )
	{
	MapLayer *pLayer = (MapLayer*)pFlowContext->pEnvContext->pMapLayer;

	MapLayer* pStreamLayer = (MapLayer*)pFlowContext->pFlowModel->m_pStreamLayer;
	
	GeoSpatialDataObj geoSpatialObj(U_UNDEFINED);

	GDALWrapper gdal;

	int reachesCount = (int) reaches->GetCount(); 

	// large number in meters
	float currentDistanceBetween = 100000000;

	int returnedComid = -1;

	for ( int i = 0; i < reachesCount; i++ )
		{
		int streamLayerComid = reaches->GetAt(i);
		
		int comidIndex = pStreamLayer->FindIndex( m_colStrLayComid, streamLayerComid, 0 );
		
		Poly *pStreamLayerPoly = pStreamLayer->GetPolygon( comidIndex );
		Poly *pIDUPoly = pLayer->GetPolygon(idu);

		Vertex vStream, vIDU;
		vStream = pStreamLayerPoly->GetCentroid( );
		
		vIDU = pIDUPoly->GetCentroid();

		CString projectionWKT = pLayer->m_projection; 
				
		int utmZone = geoSpatialObj.GetUTMZoneFromPrjWKT( projectionWKT );

		double reachLat, reachLon, iduLat, iduLon;

		// 22 = WGS 84 EquatorialRadius, and 1/flattening.
		gdal.UTMtoLL( 22,vStream.y,vStream.x,utmZone,reachLat,reachLon );
		gdal.UTMtoLL( 22,vIDU.y,vIDU.x,utmZone,iduLat,iduLon );

		// in meters
		char units = 'm';

		float distanceBetween = (float) gdal.DistanceBetween( reachLat, reachLon, iduLat, iduLon, units );

		if ( distanceBetween < currentDistanceBetween && distanceBetween < m_dynamicWRRadius )
			{
			currentDistanceBetween = distanceBetween;
			
			returnedComid = streamLayerComid;
			
			}

		}

	return returnedComid;
	}

bool AltWaterMaster::AddWaterRight(FlowContext *pFlowContext, int idu, int streamLayerComid)
   {
   if (streamLayerComid < 0) return(false);

   MapLayer *pLayer = (MapLayer*)pFlowContext->pEnvContext->pMapLayer;

   MapLayer* pStreamLayer = (MapLayer*)pFlowContext->pFlowModel->m_pStreamLayer;

   GDALWrapper gdal;

   GeoSpatialDataObj geoSpatialObj(U_UNDEFINED);

   // if the value specified in the .xml file is -1, then appropriation year is current year.
   if (m_dynamicWRAppropriationDate = -1)
      m_dynamicWRAppropriationDate = pFlowContext->pEnvContext->currentYear;

   int errorNum = 0;

   float iduAreaM2 = 0.0;
   int   iduIndex = -1;
   int   wrexist = 0;

   pLayer->GetData(idu, m_colAREA, iduAreaM2);
   pLayer->GetData(idu, m_colIDUIndex, iduIndex);
   pLayer->GetData(idu, m_colWREXISTS, wrexist);
   float iduAreaAc = iduAreaM2 * ACRE_PER_M2; // acres

   __int32 newWRexists = 0;
   //SetUse( newWRexists, (unsigned __int16) 16 ); // irrigation 
   //SetPermit( newWRexists, (unsigned __int16) 2 ); // surface water right
   newWRexists = wrexist | 16 << 8;
   newWRexists = newWRexists | 2;

   pLayer->m_readOnly = false;
   pLayer->SetData(idu, m_colWREXISTS, newWRexists); // set bitwise WREXISTS attribute in IDU layer
   pLayer->SetData(idu, m_colWR_IRRIG_S, 1); 
   pLayer->m_readOnly = true;

   int comidIndex = pStreamLayer->FindIndex(m_colStrLayComid, streamLayerComid, 0);

   Poly *pStreamLayerPoly = pStreamLayer->GetPolygon(comidIndex);
   Poly *pIDUPoly = pLayer->GetPolygon(idu);

   Vertex vStream, vIDU;
   vStream = pStreamLayerPoly->GetCentroid();

   vIDU = pIDUPoly->GetCentroid();

   // m_maxRate is in cfs/acre and specified in .xml file
   float appropriatedPODRate = m_maxRate * iduAreaAc / FT3_PER_M3; // cfs/acre to m3/sec

   WaterRight* newWaterright = new WaterRight;

   CString projectionWKT = pLayer->m_projection;

   int utmZone = geoSpatialObj.GetUTMZoneFromPrjWKT(projectionWKT);

   double reachLat, reachLon, iduLat, iduLon;

   // 22 = WGS 84 EquatorialRadius, and 1/flattening.
   gdal.UTMtoLL(22, vStream.y, vStream.x, utmZone, reachLat, reachLon);
   gdal.UTMtoLL(22, vIDU.y, vIDU.x, utmZone, iduLat, iduLon);

   // in meters
   char units = 'm';

   float distanceBetween = (float)gdal.DistanceBetween(reachLat, reachLon, iduLat, iduLon, units);

   // define water right Point of Diversion (POD) attributes and add to POD data object
   newWaterright->m_wrID = idu;
   newWaterright->m_xCoord = vStream.x;
   newWaterright->m_yCoord = vStream.y;
   newWaterright->m_podID = -comidIndex;
   newWaterright->m_pouID = -idu;
   newWaterright->m_appCode = 1;
   newWaterright->m_permitCode = WRP_SURFACE;
   newWaterright->m_podRate = appropriatedPODRate;
   newWaterright->m_useCode = WRU_IRRIGATION;
   newWaterright->m_supp = 0; //primary
   newWaterright->m_priorDoy = 1;
   newWaterright->m_priorYr = m_dynamicWRAppropriationDate; //this will be the junior water right
   newWaterright->m_beginDoy = 121; // may 1st
   newWaterright->m_endDoy = 273; // september 30th
   newWaterright->m_podStatus = WRPS_NONCANCELED;
   newWaterright->m_reachComid = streamLayerComid;
   newWaterright->m_podUseRate = appropriatedPODRate;
   newWaterright->m_distanceToReach = distanceBetween; //need to get distance out of GetNearbyPolys above

   int index = pStreamLayer->FindIndex(m_colStrLayComid, streamLayerComid, 0);
   if (index < 0) return(false); // If the reach isn't in the current stream layer, don't add the water right.

   newWaterright->m_pReach = pFlowContext->pFlowModel->FindReachFromIndex(index);

   for (int s = 0; s < m_podArray.GetSize(); s++)
      {
      WaterRight * existingWR = m_podArray[s];
      if (newWaterright->m_priorYr < existingWR->m_priorYr)
         {
         m_podArray.InsertAt(s, newWaterright);
         break;
         }
      }

   // define water right Place of Use (POU) attributes and add to POU data object

   //Build the Key for the map lookup
   m_pouInsertKey.pouID = -idu;

   int lastNdxPos = m_pouDb.GetRowCount() - 1;

   int pouDBNdx = 0;

   //Result from the Map lookup
   m_pouDb.Get(m_colPouDBNdx, lastNdxPos, pouDBNdx);

   pouDBNdx++;

   //Build the Map
   m_pouInputMap[m_pouInsertKey].push_back(pouDBNdx);

   CArray<VData, VData> newPouRow;
   newPouRow.SetSize(7, 1);

   newPouRow[0] = pouDBNdx; // index for finding POU
   newPouRow[1] = -idu; // POUID that relates to POD list
   newPouRow[2] = iduIndex; // relates to IDU_INDEX
   newPouRow[3] = iduAreaM2; //m2
   newPouRow[4] = 100; // ratio of IDU area/ POU area
   newPouRow[5] = newWaterright->m_useCode; // USECODE
   newPouRow[6] = newWaterright->m_permitCode; // PERMITCODE

   m_pouDb.AppendRow(newPouRow);

   return true;
   } // end of AddWaterRight()


bool AltWaterMaster::OutputUpdatedWRLists()
   {
   ofstream outputFile;

   outputFile.open("\\Envision\\testPOD.csv");

   outputFile << "m_wrID ,m_xCoord ,m_yCoord,m_podID,m_pouID,m_appCode ,m_permitCode ,m_podRate ,m_useCode ,m_supp ,m_priorDoy,m_priorYr,m_beginDoy,m_endDoy,m_podStatus,m_reachComid,m_podUseRate,m_distanceToReach" << endl;

   for (int i = 0; i < (int)m_podArray.GetSize(); i++)
      {
      WaterRight* wrRight = new WaterRight;
      wrRight = m_podArray[i];

      outputFile << wrRight->m_wrID << "," << wrRight->m_xCoord << "," << wrRight->m_yCoord << "," << wrRight->m_podID << "," << wrRight->m_pouID << "," << wrRight->m_appCode << "," << wrRight->m_permitCode << "," << wrRight->m_podRate << "," << wrRight->m_useCode << "," << wrRight->m_supp << "," << wrRight->m_priorDoy << "," << wrRight->m_priorYr << "," << wrRight->m_beginDoy << "," << wrRight->m_endDoy << "," << wrRight->m_podStatus << "," << wrRight->m_reachComid << "," << wrRight->m_podUseRate << "," << wrRight->m_distanceToReach << endl;
      }

   outputFile.close();

   ofstream outputFile2;

   outputFile2.open("\\Envision\\testPOU.csv");

   outputFile2 << "POU_INDEX ,POUID,	IDU_INDEX, AREA, PERCENT" << endl;

   for (int i = 0; i < (int)m_pouDb.GetRowCount(); i++)
      {
      int pouIndex = 0;
      int pouID = 0;
      int iduIndex = 0;
      float area = 0.0;
      float percent = 0.0;

      m_pouDb.Get(m_colPouDBNdx, i, pouIndex);
      m_pouDb.Get(m_colPUPouID, i, pouID);
      m_pouDb.Get(m_colPouIDU_INDEX, i, iduIndex);
      m_pouDb.Get(m_colPouArea, i, area);
      m_pouDb.Get(m_colPouPct, i, percent);

      outputFile2 << pouIndex << "," << pouID << "," << iduIndex << "," << area << "," << percent << endl;
      }

   outputFile2.close();

   return true;
   }

bool AltWaterMaster::ExportDistPodComid(FlowContext *pFlowContext, char unit)
	{
	// If the stream layer "resolution" is less than the POD layer, then the user might want to
	// quantify the distance between a POD and the closest vertex of the "polyline" representing
	// a reach.  the output is the same size and indexed the same as the input data file as read
	// in by the method, LoadWRDatabase

	GDALWrapper gdal;

	GeoSpatialDataObj geoSpatialObj(U_UNDEFINED);

	MapLayer *pStreamLayer = (MapLayer*)pFlowContext->pFlowModel->m_pStreamLayer;

	MapLayer *pLayer = (MapLayer*)pFlowContext->pEnvContext->pMapLayer;

	Poly *reachPoly;

	ofstream outputFile;

	outputFile.open("\\Envision\\distanceBetweenPodComid.csv");

	if ( !outputFile )
		{
		CString msg;
	   msg.Format( "AltWM::ExportDistPodComid: cannot open output file" );
	   Report::ErrorMsg( msg );
		return false;
		}

	outputFile << "COMID,PODX,PODY,Distance" << endl;

	for (int i = 0; i < (int)m_podArray.GetSize(); i++)
		{
		// Get current WR
		WaterRight *pRight = m_podArray[i];

		int comid = pRight->m_reachComid;

		double podX = pRight->m_xCoord;

		double podY = pRight->m_yCoord;

		int comidIndex = pStreamLayer->FindIndex( m_colStrLayComid, comid, 0 );
		
		if ( comidIndex < 0 ) 
			{
			outputFile << comid << "," << podX << "," << podY << ",-99"<< endl;
			continue; // for -99 comids (outside basin) and subbasin issues
			}

		reachPoly = pStreamLayer->GetPolygon( comidIndex );

		int vertexCount = reachPoly->GetVertexCount();
		
		float closestDistance = 10000000;  // a large number

		for (int v = 0; v < vertexCount; v++)
			{           
			if ( reachPoly->GetVertexCount() > 0 )
				{
			   double reachX = reachPoly->GetVertex( v ).x;  
			   double reachY = reachPoly->GetVertex( v ).y;
				
				double reachLat = 0.0;
				double reachLon = 0.0;
				double podLat   = 0.0;
				double podLon   = 0.0;
			   
				CString projectionWKT = pLayer->m_projection; 
				
				int utmZone = geoSpatialObj.GetUTMZoneFromPrjWKT( projectionWKT );

				// 22 = WGS 84 EquatorialRadius, and 1/flattening.  UTM zone 10.
				gdal.UTMtoLL( 22,reachY,reachX,utmZone,reachLat,reachLon );
				gdal.UTMtoLL( 22,podY,podX,utmZone,podLat,podLon );
				gdal.UTMtoLL( 22,podY,podX,10,podLat,podLon );

				double distanceBetween = gdal.DistanceBetween( reachLat,reachLon,podLat,podLon,unit );

				if ( distanceBetween < closestDistance ) 
					closestDistance = (float) distanceBetween;
			   }
			else
			   {
			   CString msg;
			   msg.Format( "AltWM::ExportDistPodComid Bad stream poly (id=%i) - no vertices!!!", reachPoly->m_id );
			   Report::ErrorMsg( msg );
			   }
			}	// endfor vertices
		
		outputFile << comid << "," << podX << "," << podY << ","<<closestDistance<< endl;
			
		} // endfor POD lookup table
	
	outputFile.close();

	return true;
	}

int AltWaterMaster::ApplyRegulation(FlowContext *pFlowContext, int regulationType, WaterRight *pWaterRight, int depth, float deficit)
	{

	int nAffected = 0;

	if (depth>=0) switch ( regulationType )
      {

      case 1:
		   
         nAffected = RegulatoryShutOff(pFlowContext, pWaterRight, m_recursiveDepth, deficit);

         break;

      case 2:
		   
         nAffected = NoteJuniorWaterRights(pFlowContext, pWaterRight, m_recursiveDepth, deficit);

         break;

      default:
         
			//no regulation
         break;

      }

	return nAffected;
	}


int AltWaterMaster::RegulatoryShutOff( FlowContext *pFlowContext, WaterRight *pWaterRight, int depth , float deficit )
   {
   MapLayer *pLayer = (MapLayer*)pFlowContext->pEnvContext->pMapLayer;
   int doy1 = pFlowContext->dayOfYear + 1; // day of year, with Jan 1 = 1
   int index = 0;

	Reach *pReach = pWaterRight->m_pReach;
		
	CArray<WaterRight*, WaterRight*> *wrArray = new CArray<WaterRight*, WaterRight*>;   // create an initial, empty array for holding any found junior water rights in this reach and upstream reaches

   int count = GetJuniorWaterRights(pFlowContext, pReach, pWaterRight, depth, wrArray);  // get list of all water rights more junior to the target WR that are in this reach or upstream reaches
   int num_IDU_shutoffs = 0;
	while ( deficit > 0 && index < count )  // iterate through junior rights until deficit is covered.
		{    
		// this array was sorted from junior to senior as it was built in GetJuniorWaterRights
		WaterRight *pNext = wrArray->GetAt(index);
      index++;

		// if surface water right for irrigation, has not been seen this step, is "in-season", has not been shut off yet this season, and m_stepRequest > 0
      // then simulate regulatory shut off
      if (!(pNext->m_permitCode == WRP_SURFACE && pNext->m_useCode == WRU_IRRIGATION &&
         pNext->m_stepShortageFlag == false && pNext->m_stepRequest > 0.f && !pNext->m_suspendForYear &&
         (pFlowContext->dayOfYear >= pNext->m_beginDoy && pFlowContext->dayOfYear <= pNext->m_endDoy)))
         continue;
      pNext->m_suspendForYear = true;

		// object to hold idus for this WR
		vector<int> *pouIDUs = 0;	

		// object for getting IDU_INDEX in sub area				     
		vector<int> *iduNdxVec = 0;

		// Get POUID (aka Place of Use) for current Permit
		int pouID = pNext->m_pouID;

		// Build lookup key into point of use (POU) map/lookup table
		m_pouLookupKey.pouID = pouID;

		// Returns vector of indexs into the POU input data file. Used for relating to current water right POU to polygons in IDU layer
		pouIDUs = &m_pouInputMap[m_pouLookupKey];

		// if a PODID does not have a POUID, consider next WR
      if (pouIDUs->size() <= 0)  continue;

      // Regulatory action: shut off this water right for the remainder of the season.

      // Assumes today's step request will be the same as yesterday's step request
      deficit -= pNext->m_stepRequest; // m3/sec reduce the deficit appropriately
      pNext->m_stepShortageFlag = true;

      // Set Begin looping through IDUs Associated with WRID and PLace of Use (POU)
      for (int j = 0; j < (int)pouIDUs->size(); j++)
         {
         int tempIDUNdx = m_pouDb.GetAsInt(m_colPouIDU_INDEX, pouIDUs->at(j));

         //build key into idu index map
         m_iduIndexLookupKey.iduIndex = tempIDUNdx;

         //returns vector with the idu index for idu layer
         iduNdxVec = &m_IDUIndexMap[m_iduIndexLookupKey];

         int iduNdx = 0;

         //if no index is return, then idu the POU is associated with is not in current layer
         if (iduNdxVec->size() == 0) continue;

         iduNdx = iduNdxVec->at(0);
         pLayer->m_readOnly = false;
         pLayer->SetData(iduNdx, m_colWRShutOff, 1);
         pLayer->m_readOnly = true;
         num_IDU_shutoffs++;
         } // end of for (int j = 0; j < (int)pouIDUs->size(); j++)
		} // end of while ( deficit > 0 && index < count )
   return num_IDU_shutoffs;  
   } // end of RegulatoryShutOff()

int AltWaterMaster::NoteJuniorWaterRights( FlowContext *pFlowContext, WaterRight *pWaterRight, int depth, float deficit )
   {	
	MapLayer *pLayer = (MapLayer*)pFlowContext->pEnvContext->pMapLayer;
   int doy1 = pFlowContext->dayOfYear + 1; // day of year, with Jan 1 = 1
	int index = 0;
	
	// set .xml file.  only proceed if there is a threshold for suspending a WR (0-365)
	if ( m_maxDaysShortage >= 0 )
		{
		
		Reach *pReach = pWaterRight->m_pReach;
		
		CArray<WaterRight*, WaterRight*> *wrArray = new CArray<WaterRight*, WaterRight*>;   // create an initial, empty array for holding any found junior water rights in this reach and upstream reaches

      int count = GetJuniorWaterRights(pFlowContext, pReach, pWaterRight, depth, wrArray);  // get list of all water rights more junior to the target WR that are in this reach or upstream reaches
 
		while ( deficit > 0 && index < count )  // iterate through junior rights until deficit is covered.
		   {    
			// this array was sorted from junior to senior as it was built in GetJuniorWaterRights
			WaterRight *pNext = wrArray->GetAt(index);

         // if surface water right for irrigation, has not been seen this step, and is "in-season", and m_stepRequest > 0
         if (pNext->m_permitCode == WRP_SURFACE && pNext->m_useCode == WRU_IRRIGATION &&
            pNext->m_stepShortageFlag == false && pNext->m_stepRequest > 0.f &&
				  (  doy1 >= pNext->m_beginDoy && doy1 <= pNext->m_endDoy  ) )
				{
				// object to hold idus for this WR
				vector<int> *pouIDUs = 0;	

				// object for getting IDU_INDEX in sub area				     
				vector<int> *iduNdxVec = 0;

				// figure out flow increase when we cut this one off;
				// Assumes next step request is the same as this step request
				float flowFromNextRight = pNext->m_stepRequest;

				deficit -= flowFromNextRight; // m3/sec reduce the deficit appropriately

				pNext->m_stepShortageFlag = true;

				// Get POUID (aka Place of Use) for current Permit
		      int pouID = pNext->m_pouID;

				// Build lookup key into point of use (POU) map/lookup table
				m_pouLookupKey.pouID = pouID;

				// Returns vector of indexs into the POU input data file. Used for relating to current water right POU to polygons in IDU layer
				pouIDUs = &m_pouInputMap[m_pouLookupKey];

				// if a PODID does not have a POUID, consider next WR
            if (pouIDUs->size() == 0) continue;

				// Set action level. Begin looping through IDUs Associated with WRID and PLace of Use (POU)
				for (int j = 0; j < (int) pouIDUs->size(); j++)
					{
               int tempIDUNdx = m_pouDb.GetAsInt(m_colPouIDU_INDEX, pouIDUs->at(j));

					//build key into idu index map
					m_iduIndexLookupKey.iduIndex = tempIDUNdx;

					//returns vector with the idu index for idu layer
					iduNdxVec = &m_IDUIndexMap[m_iduIndexLookupKey];

					int iduNdx = 0;

					//if no index is return, then idu the POU is associated with is not in current layer
               if (iduNdxVec->size() == 0) continue;

					iduNdx = iduNdxVec->at(0);

					/* Get the area of the POU for current WRid and IDU_INDEX
			      float areaPou = m_pouDb.GetAsFloat(m_colPouArea, pouIDUs->at(j));

			      float iduAreaM2 = 0.0f;

			      pLayer->GetData( iduNdx, m_colAREA, iduAreaM2 );

					float pctIDUarea = areaPou / iduAreaM2 * 100;
			
			      // check threshold of necessary intersection 
					if ( pctIDUarea < m_pctIDUPOUIntersection ) continue;
               */
					pLayer->m_readOnly = false;
					pLayer->SetData( iduNdx, m_colWRJuniorAction, 1 ); // level 1 WR_JRNOTES
					pLayer->m_readOnly = true;
/*
					int consecCheck =  pFlowContext->dayOfYear - pNext->m_lastDOYShortage; 

					( consecCheck < 2 ) ? pNext->m_stepsSuspended++ : pNext->m_stepsSuspended = 0;
				
					if ( pNext->m_stepsSuspended >= m_maxDaysShortage )
						{					
						pNext->m_suspendForYear = false;

						pLayer->m_readOnly = false;
						pLayer->SetData( iduNdx, m_colWRJuniorAction, 2 ); // level 2
						pLayer->m_readOnly = true;
						}
*/
					}

				pNext->m_lastDOYShortage = pFlowContext->dayOfYear; 

				}
		   index++;
		   }
		}
   return index;  // number of junior rights cut off
   }

int AltWaterMaster::GetJuniorWaterRights( FlowContext *pFlowContext,  Reach *pReach, WaterRight *pWaterRight, int depth, CArray<WaterRight*, WaterRight*> *wrArray )
   {
	// note: calling this initially with depth=0 will look only at the reach; values greater than 0 
	// cause it to look increasngly further upstream.

	// terminate recursion?
   if ( depth < 0 )
      return (int) wrArray->GetSize();

	MapLayer* pStreamLayer = (MapLayer*) pFlowContext->pFlowModel->m_pStreamLayer;

   // get the water rights associated with this reach
	CArray<WaterRight*, WaterRight*> *thisWRArray = new CArray<WaterRight*, WaterRight*>;
   CArray< Reach*, Reach* > *thisReachArray = new CArray< Reach*, Reach* >;

	thisReachArray->Add( pReach );

 	int nWR = GetWRsInReach( thisReachArray, thisWRArray );

   // this would need to be created when the wr database is loaded.
   // if there are any juniors, put them into the cumulative array
   for ( int i=0; i < (int) thisWRArray->GetSize(); i++ )
      {
      WaterRight * _pWaterRight = thisWRArray->GetAt( i );

		// do not consider the seed water right
		if ( _pWaterRight->m_wrID == pWaterRight->m_wrID && _pWaterRight->m_podID == pWaterRight->m_podID )
			continue;

		bool isJunior = IsJunior( pWaterRight, _pWaterRight );
		
      // If the candidate WR is in the same reach as the original WR, consider it to be downstream.
      bool isUpStream = pWaterRight->m_reachComid != _pWaterRight->m_reachComid;

      // compare to the initial water right experiencing a shortage
		if ( isJunior && isUpStream ) 	
			{
			if ( wrArray->GetSize() == 0 )
				{
				wrArray->Add( _pWaterRight );
				}
			else // sort as you go from junior to senior
				{				 
				for ( int s = 0; s < wrArray->GetSize(); s++ )
					{
					
					WaterRight * existingWR = wrArray->GetAt( s );
					
				   bool isJunior = IsJunior( existingWR, _pWaterRight );

					if ( isJunior  )
					   {			
						wrArray->InsertAt( s, _pWaterRight );
						break;
						}
					else if ( s == (wrArray->GetSize() - 1) && !isJunior )
						{
						wrArray->Add( _pWaterRight );
						}
					} 
				}
			}
      }
     
	// recurse upstream if needed
	Reach *pUpstreamReachLeft = NULL;
	Reach *pUpstreamReachRight = NULL;

	if (pReach->m_pLeft != NULL)
		{
		int reachIDLeft = pReach->m_pLeft->m_reachID;
		int index = pStreamLayer->FindIndex(m_colStrLayComid, reachIDLeft, 0);
		pUpstreamReachLeft = pFlowContext->pFlowModel->FindReachFromIndex(index);	
		}

	if (pReach->m_pRight != NULL)
		{
		int reachIDRight = pReach->m_pRight->m_reachID;
		int index = pStreamLayer->FindIndex(m_colStrLayComid, reachIDRight, 0);
		pUpstreamReachRight = pFlowContext->pFlowModel->FindReachFromIndex(index);		
		}
		
	if (pUpstreamReachLeft != NULL)
		{
		GetJuniorWaterRights( pFlowContext, pUpstreamReachLeft, pWaterRight, depth-1, wrArray );
		}

	if (pUpstreamReachRight != NULL)
		{
		GetJuniorWaterRights( pFlowContext, pUpstreamReachRight,  pWaterRight, depth-1, wrArray );
		}

   return (int) wrArray->GetSize();
   }

bool AltWaterMaster::IsJunior(WaterRight *incumbentWR, WaterRight *canidateWR)
	{
	bool isJunior = false;		
	
	if ( canidateWR->m_priorYr >= incumbentWR->m_priorYr )
		{
		isJunior = true;
	
		if ( canidateWR->m_priorYr == incumbentWR->m_priorYr && canidateWR->m_priorDoy < incumbentWR->m_priorDoy )
			{
			isJunior = false;
			}
		}
	return isJunior;
	}

int  AltWaterMaster::GetWRsInReach( CArray<Reach*, Reach*> *reachArray, CArray<WaterRight*, WaterRight*> *wrArray )
	{
	
	for ( int i = 0; i < (int) reachArray->GetSize(); i++ )
		{
		
		Reach *pReach = reachArray->GetAt( i );

		for ( int j = 0; j < (int) m_podArray.GetSize(); j++ )
			{
			WaterRight *tmpRight = m_podArray[j];

			int reachID = tmpRight->m_reachComid;

			if ( pReach->m_reachID == tmpRight->m_reachComid )
				wrArray->Add( tmpRight );
			}
		}

	return (int) wrArray->GetSize();
	}

bool AltWaterMaster::IsUpStream(FlowContext *pFlowContext, WaterRight *incumbentWR, WaterRight *canidateWR)
	{
	MapLayer *pLayer = (MapLayer*)pFlowContext->pEnvContext->pMapLayer;

	MapLayer* pStreamLayer = (MapLayer*)pFlowContext->pFlowModel->m_pStreamLayer;
	
	CString projectionWKT = pLayer->m_projection; 

	bool isUpStream = false;

	int currentStreamLayerComid = incumbentWR->m_reachComid;

	int comidIndex = pStreamLayer->FindIndex( m_colStrLayComid, currentStreamLayerComid, 0 );
	
	Poly *pCurrentStreamLayerPoly = pStreamLayer->GetPolygon( comidIndex );

	Vertex currentCentroid;

	currentCentroid = pCurrentStreamLayerPoly->GetCentroid( );
	
	double x = currentCentroid.x;
   double y = currentCentroid.y;
 	
	int twoDindexvalue = 0;	

	int set3Dvalue = 0;
 
	short int incumbentWRElevation = this->m_myGeo->GetAsShortInt( x, y, twoDindexvalue, set3Dvalue, projectionWKT, true );

	currentStreamLayerComid = canidateWR->m_reachComid;

	comidIndex = pStreamLayer->FindIndex( m_colStrLayComid, currentStreamLayerComid, 0 );
	
	pCurrentStreamLayerPoly = pStreamLayer->GetPolygon( comidIndex );

	currentCentroid = pCurrentStreamLayerPoly->GetCentroid( );
	
	x = currentCentroid.x;
   y = currentCentroid.y;
 	
	twoDindexvalue = 0;	

	set3Dvalue = 0;
 
	short int canidateWRElevation = this->m_myGeo->GetAsShortInt( x, y, twoDindexvalue, set3Dvalue, projectionWKT, true );
				
	// if equal to, assumes stagnant water and incumbentWR draw down affects canidateWR
	if ( canidateWRElevation >= incumbentWRElevation )
		isUpStream = true;

	return isUpStream;
	}


bool AltWaterMaster::LoadXml(WaterAllocation *pMethod, TiXmlElement *pXmlElement, LPCTSTR filename )
   {
   LPTSTR query    = NULL;   // ignored for now
   LPTSTR method   = NULL;
   LPTSTR podTable = NULL;
   LPTSTR pouTable = NULL;
	LPTSTR dynamicWRTable = NULL;
   LPTSTR value    = NULL;
   LPTSTR aggCols  = NULL;
   int irrigatedHruLayer = 0;
	int nonIrrigatedHruLayer = 0;
   int nYearsWRNotUsedLimit = 0;
   int nDaysWRConflict1 = 0;
   int nDaysWRConflict2 = 0;
	float defaultMaxDuty = 0.0;
	float maxIrrDiversion = 0.0;
	float fracDischargeAvail = 0.0;
	float maxDistanceToReach = 0.0;
	int   exportDisPodComid = 0;
	float dynamicWRRadius = 0.0;
	int maxDaysShortage = 365;
	int recursiveDepth = 1;
	float pctIDUPOUIntersection = 60.0;
	int dynamAppropriationDate = -1;

	bool  wmDebug = false;
	int   maxDutyHalt = 0;
   float defaultMaxRate = (float) 0.0125 / FT3_PER_M3;  // cfs, convert to m3/sec
   LPTSTR minFlowColName = NULL;
   LPTSTR WRExistColName = NULL;
   LPTSTR irrigateColName = NULL;
	LPTSTR maxDutyColName = NULL;
	int irrBeginDoy = 364; // if initial irr default begin and end DOY are 364, no diversions will happen 
	int irrEndDoy = 364;
   bool use = true;

    XML_ATTR attrs[] = {
		// attr                                   type				address						isReq  checkCol
      { "name",											TYPE_CSTRING,  &(pMethod->m_name),		false,   0 },
      { "method",											TYPE_STRING,   &method,						true,    0 },
      { "query",											TYPE_STRING,   &query,						false,   0 },
      { "pod_table",										TYPE_STRING,   &podTable,					true,    0 },
      { "pou_table",										TYPE_STRING,   &pouTable,					true,    0 },
		{ "dynamic_WR_table",							TYPE_STRING,   &dynamicWRTable,			true,    0 },
      { "irrigated_hrulayer",							TYPE_INT,      &irrigatedHruLayer,		true,    0 },
		{ "nonIrrigated_hrulayer",						TYPE_INT,		&nonIrrigatedHruLayer,	true,    0 },
      { "n_years_WR_not_used_limit",				TYPE_INT,      &nYearsWRNotUsedLimit,	true,    0 },
      { "n_days_WR_shortage_1",						TYPE_INT,      &nDaysWRConflict1,		true,    0 },
      { "n_days_WR_shortage_2",						TYPE_INT,      &nDaysWRConflict2,		true,    0 },
		{ "export_distance_POD_COMID",				TYPE_INT,      &exportDisPodComid,		true,    0 },
		{ "default_max_duty",                     TYPE_FLOAT,    &defaultMaxDuty,        true,    0 },
	   { "fraction_discharge_available",         TYPE_FLOAT,    &fracDischargeAvail,    true,    0 },
		{ "percent_IDU_POU_intersection",         TYPE_FLOAT,    &pctIDUPOUIntersection, true,    0 },
		{ "max_duty_halt",								TYPE_INT,      &maxDutyHalt,		      true,    0 },
		{ "default_max_rate",                     TYPE_FLOAT,    &defaultMaxRate,        false,   0 },
		{ "dynamicWRRadius",                      TYPE_FLOAT,    &dynamicWRRadius,       false,   0 },
		{ "daily_maximum_allowed_irrigtion",		TYPE_FLOAT,    &maxIrrDiversion,       false,   0 },
		{ "max_days_in_shortage",						TYPE_INT,      &maxDaysShortage,		   true,    0 },
		{ "reach_recursive_depth_shortage",			TYPE_INT,      &recursiveDepth,		   true,    0 },
		{ "max_distance_pod_reach",					TYPE_FLOAT,    &maxDistanceToReach,    false,   0 },
      { "min_flow_col_name",							TYPE_STRING,   &minFlowColName,			false,   0 },
      { "Water_RightExist_BitWise_col_name",		TYPE_STRING,   &WRExistColName,			false,   0 },
      { "irrigate_or_not_col_name",					TYPE_STRING,	&irrigateColName,			false,   0 },
		{ "max_duty_col_name",                    TYPE_STRING,   &maxDutyColName,        false,   0 },
		{ "default_irrigation_begin_dayofyear",	TYPE_INT,		&irrBeginDoy,				false,	0 },
	   { "default_irrigation_end_dayofyear",		TYPE_INT,      &irrEndDoy,					false,   0 },
      { "value",											TYPE_STRING,   value,						false,   0 },
		{ "DynamicWR_Appropriation_Date",         TYPE_INT,      &dynamAppropriationDate,false,   0 },
      { "agg_cols",										TYPE_STRING,   &aggCols,					false,   0 },
      { "use",												TYPE_BOOL,     &use,							false,   0 },
		{ "debug",											TYPE_BOOL,     &wmDebug,					false,   0 },
      { NULL,												TYPE_NULL,     NULL,							false,   0 } };


   bool ok = TiXmlGetAttributes( pXmlElement, attrs, filename );
   if ( ! ok )
      {
      CString msg; 
      msg.Format( _T("Flow: Misformed element reading <allocation> attributes for AltWaterMaster method in input file %s"), filename );
      Report::ErrorMsg( msg );
      delete pMethod;
      return false;
      }

   m_podTablePath = podTable;
   m_pouTablePath = pouTable;
   m_dynamicWRTablePath = dynamicWRTable;
   m_irrigatedHruLayer = irrigatedHruLayer;
   m_nonIrrigatedHruLayer = nonIrrigatedHruLayer;
   m_nDaysWRConflict1 = nDaysWRConflict1;
   m_nDaysWRConflict2 = nDaysWRConflict2;
   m_maxDuty = defaultMaxDuty;
   m_maxDutyHalt = maxDutyHalt;
   m_maxRate = defaultMaxRate;
   m_fractionDischargeAvail = fracDischargeAvail;
   m_exportDistPodComid = exportDisPodComid;
   m_maxIrrDiversion = maxIrrDiversion;
   m_maxDistanceToReach = maxDistanceToReach;
   m_minFlowColName = minFlowColName;
   m_wrExistsColName = WRExistColName;
   m_irrigateColName = irrigateColName;
   m_maxDutyColName = maxDutyColName;
   m_nYearsWRNotUsedLimit = nYearsWRNotUsedLimit;
   m_irrDefaultBeginDoy = irrBeginDoy;
   m_irrDefaultEndDoy = irrEndDoy;
   m_debug = wmDebug;
   m_dynamicWRRadius = dynamicWRRadius;
   m_maxDaysShortage = maxDaysShortage;
   m_recursiveDepth = recursiveDepth;
   m_dynamicWRAppropriationDate = dynamAppropriationDate;
   m_pctIDUPOUIntersection = pctIDUPOUIntersection;
   pMethod->m_pAltWM = this;   

   return true;
   }
