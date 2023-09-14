/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This file incorporates work covered by the following license notice:
 *
 *   Licensed to the Apache Software Foundation (ASF) under one or more
 *   contributor license agreements. See the NOTICE file distributed
 *   with this work for additional information regarding copyright
 *   ownership. The ASF licenses this file to you under the Apache
 *   License, Version 2.0 (the "License"); you may not use this file
 *   except in compliance with the License. You may obtain a copy of
 *   the License at http://www.apache.org/licenses/LICENSE-2.0 .
 */
#include <config_wasm_strip.h>
#include <wrtsh.hxx>
#include <pam.hxx>
#include <node.hxx>
#include <ndtxt.hxx>
#include <translatehelper.hxx>
#include <sal/log.hxx>
#include <rtl/string.h>
#include <shellio.hxx>
#include <vcl/scheduler.hxx>
#include <vcl/svapp.hxx>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <vcl/htmltransferable.hxx>
#include <vcl/transfer.hxx>
#include <swdtflvr.hxx>
#include <linguistic/translate.hxx>
#include <com/sun/star/task/XStatusIndicator.hpp>
#include <sfx2/viewfrm.hxx>
#include <com/sun/star/task/XStatusIndicatorFactory.hpp>
#include <strings.hrc>

namespace SwTranslateHelper
{
OString ExportPaMToHTML(SwPaM* pCursor)
{
    SolarMutexGuard gMutex;
    OString aResult;
    WriterRef xWrt;
    GetHTMLWriter(u"NoLineLimit,SkipHeaderFooter,NoPrettyPrint", OUString(), xWrt);
    if (pCursor != nullptr)
    {
        SvMemoryStream aMemoryStream;
        SwWriter aWriter(aMemoryStream, *pCursor);
        ErrCode nError = aWriter.Write(xWrt);
        if (nError.IsError())
        {
            SAL_WARN("sw.ui", "ExportPaMToHTML: failed to export selection to HTML");
            return {};
        }
        aResult
            = OString(static_cast<const char*>(aMemoryStream.GetData()), aMemoryStream.GetSize());
        aResult = aResult.replaceAll("<p", "<span");
        aResult = aResult.replaceAll("</p>", "</span>");

        // HTML has for that <br> and <p> also does new line
        aResult = aResult.replaceAll("<ul>", "");
        aResult = aResult.replaceAll("</ul>", "");
        aResult = aResult.replaceAll("<ol>", "");
        aResult = aResult.replaceAll("</ol>", "");
        aResult = aResult.replaceAll("\n", "").trim();
        return aResult;
    }
    return {};
}

void PasteHTMLToPaM(SwWrtShell& rWrtSh, SwPaM* pCursor, const OString& rData)
{
    SolarMutexGuard gMutex;
    rtl::Reference<vcl::unohelper::HtmlTransferable> pHtmlTransferable
        = new vcl::unohelper::HtmlTransferable(rData);
    if (pHtmlTransferable.is())
    {
        TransferableDataHelper aDataHelper(pHtmlTransferable);
        if (aDataHelper.GetXTransferable().is()
            && SwTransferable::IsPasteSpecial(rWrtSh, aDataHelper))
        {
            rWrtSh.SetSelection(*pCursor);
            SwTransferable::Paste(rWrtSh, aDataHelper);
            rWrtSh.KillSelection(nullptr, false);
        }
    }
}

}
