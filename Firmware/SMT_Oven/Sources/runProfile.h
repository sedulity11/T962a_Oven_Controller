/*
 * runProfile.h
 *
 *  Created on: 28 Sep 2016
 *      Author: podonoghue
 */

#ifndef SOURCES_RUNPROFILE_CPP_
#define SOURCES_RUNPROFILE_CPP_

namespace RunProfile {

/**
 * Draw profile to LCD
 *
 * @param profile The profile to draw
 */
extern void drawProfile(const NvSolderProfile &profile);

/**
 * Run profile
 *
 * @param profile The profile to run
 */
extern void run(const NvSolderProfile &profile);

/**
 * Monitor thermocouple status
 * Also allows thermocouples to be disabled
 */
extern void monitor();

/**
 * Manually control oven
 */
extern void manualMode();

}; // namespace RunProfile

#endif /* SOURCES_RUNPROFILE_CPP_ */