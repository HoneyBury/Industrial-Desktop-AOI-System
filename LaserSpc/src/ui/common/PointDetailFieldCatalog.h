#pragma once

#include <QObject>
#include <QString>

namespace LaserSpc::Ui {

class PointDetailFieldCatalog {
public:
    static QString displayName(const QString& key) {
        if (key == QStringLiteral("version")) return QObject::tr("版本");
        if (key == QStringLiteral("data")) return QObject::tr("详情数据");
        if (key == QStringLiteral("templateFilePath")) return QObject::tr("模板文件路径");
        if (key == QStringLiteral("laserTemplatePath")) return QObject::tr("兼容模板路径");
        if (key == QStringLiteral("laserContent")) return QObject::tr("镭射内容");
        if (key == QStringLiteral("readCodeContent")) return QObject::tr("读码内容");
        if (key == QStringLiteral("success")) return QObject::tr("识别成功");
        if (key == QStringLiteral("programName")) return QObject::tr("程序名");
        if (key == QStringLiteral("startTime")) return QObject::tr("开始时间");
        if (key == QStringLiteral("endTime")) return QObject::tr("结束时间");
        if (key == QStringLiteral("fieldDisplayNames")) return QObject::tr("字段显示名映射");
        if (key == QStringLiteral("算法规划") || key == QStringLiteral("algorithmPlan")) return QObject::tr("算法规划");
        if (key == QStringLiteral("name")) return QObject::tr("节点名称");
        if (key == QStringLiteral("nodeName")) return QObject::tr("节点名称");
        if (key == QStringLiteral("ok")) return QObject::tr("成功分支");
        if (key == QStringLiteral("ng")) return QObject::tr("失败分支");
        if (key == QStringLiteral("templateRevision")) return QObject::tr("模板版本");
        if (key == QStringLiteral("cameraProfile")) return QObject::tr("相机方案");
        if (key == QStringLiteral("exposureMs")) return QObject::tr("曝光时间(ms)");
        if (key == QStringLiteral("roi")) return QObject::tr("区域范围");
        if (key == QStringLiteral("x")) return QObject::tr("X坐标");
        if (key == QStringLiteral("y")) return QObject::tr("Y坐标");
        if (key == QStringLiteral("width")) return QObject::tr("宽度");
        if (key == QStringLiteral("height")) return QObject::tr("高度");
        if (key == QStringLiteral("sequence")) return QObject::tr("节点序号");
        if (key == QStringLiteral("pointIndex")) return QObject::tr("点位序号");
        return key;
    }
};

}  // namespace LaserSpc::Ui
