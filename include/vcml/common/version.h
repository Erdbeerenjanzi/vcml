/******************************************************************************
 *                                                                            *
 * Copyright 2018 Jan Henrik Weinstock                                        *
 *                                                                            *
 * Licensed under the Apache License, Version 2.0 (the "License");            *
 * you may not use this file except in compliance with the License.           *
 * You may obtain a copy of the License at                                    *
 *                                                                            *
 *     http://www.apache.org/licenses/LICENSE-2.0                             *
 *                                                                            *
 * Unless required by applicable law or agreed to in writing, software        *
 * distributed under the License is distributed on an "AS IS" BASIS,          *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   *
 * See the License for the specific language governing permissions and        *
 * limitations under the License.                                             *
 *                                                                            *
 ******************************************************************************/

#ifndef VCML_VERSION_H
#define VCML_VERSION_H

#define VCML_VERSION 20180824

#define VCML_VERSION_MAJOR 1
#define VCML_VERSION_MINOR 0
#define VCML_VERSION_PATCH 0

#define VCML_STRINGIFY(s)   VCML_STRINGIFY2(s)
#define VCML_STRINGIFY2(s)  #s

#define VCML_RELEASE_STRING VCML_STRINGIFY(VCML_VERSION)
#define VCML_VERSION_STRING VCML_STRINGIFY(VCML_VERSION_MAJOR) "." \
                            VCML_STRINGIFY(VCML_VERSION_MINOR) "." \
                            VCML_STRINGIFY(VCML_VERSION_PATCH) "-" \
                            VCML_RELEASE_STRING

#endif
