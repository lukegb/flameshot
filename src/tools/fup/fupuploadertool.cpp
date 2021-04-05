// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2017-2019 Alejandro Sirgo Rica & Contributors

#include "fupuploadertool.h"
#include "fupuploader.h"
#include <QPainter>

FupUploaderTool::FupUploaderTool(QObject* parent)
  : AbstractActionTool(parent)
{}

bool FupUploaderTool::closeOnButtonPressed() const
{
    return true;
}

QIcon FupUploaderTool::icon(const QColor& background, bool inEditor) const
{
    Q_UNUSED(inEditor);
    return QIcon(iconPath(background) + "cloud-upload.svg");
}
QString FupUploaderTool::name() const
{
    return tr("Image Uploader");
}

ToolType FupUploaderTool::nameID() const
{
    return ToolType::IMGUR;
}

QString FupUploaderTool::description() const
{
    return tr("Upload the selection to Fup");
}

QWidget* FupUploaderTool::widget()
{
    return new FupUploader(capture);
}

CaptureTool* FupUploaderTool::copy(QObject* parent)
{
    return new FupUploaderTool(parent);
}

void FupUploaderTool::pressed(const CaptureContext& context)
{
    capture = context.selectedScreenshotArea();
    emit requestAction(REQ_CAPTURE_DONE_OK);
    emit requestAction(REQ_ADD_EXTERNAL_WIDGETS);
}
