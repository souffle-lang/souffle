/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2016, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

#pragma once

#include "ProfileEvent.h"
#include "OutputProcessor.h"
#include "htmlCssChartist.h"
#include "htmlCssStyle.h"
#include "htmlJsChartistMin.h"
#include "htmlJsChartistPlugin.h"
#include "htmlJsMain.h"
#include "htmlJsTableSort.h"
#include "htmlJsUtil.h"
#include "htmlMain.h"
#include <sstream>
#include <string>

namespace souffle {
namespace profile {

/*
 * Class linking the html, css, and js into one html file
 * so that a data variable can be inserted in the middle of the two strings and written to a file.
 *
 */
class HtmlGenerator {
public:
    static std::string getHtml(std::string json) {
        return getFirstHalf() + json + getSecondHalf();
    }

protected:
    static std::string getFirstHalf() {
        std::stringstream ss;
        ss << html::htmlHeadTop << HtmlGenerator::wrapCss(html::cssChartist) << wrapCss(html::cssStyle)
           << html::htmlHeadBottom << html::htmlBodyTop << "<script>data=";
        return ss.str();
    }
    static std::string getSecondHalf() {
        std::stringstream ss;
        ss << "</script>" << wrapJs(html::jsTableSort) << wrapJs(html::jsChartistMin)
           << wrapJs(html::jsChartistPlugin) << wrapJs(html::jsUtil) << wrapJs(html::jsMain)
           << html::htmlBodyBottom;
        return ss.str();
    }
    static std::string wrapCss(const std::string& css) {
        return "<style>" + css + "</style>";
    }
    static std::string wrapJs(const std::string& js) {
        return "<script>" + js + "</script>";
    }
};

}  // namespace profile
}  // namespace souffle
