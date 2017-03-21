#include "ABCViewer.h"
#include <ai_nodes.h>

#include <vector>

#include "attributes/AttrHelper.h"

#include "common/UtilityFunctions.h"

#include <maya/MBoundingBox.h>
#include <maya/MDagPath.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MPlugArray.h>
#include <maya/MRenderUtil.h>
#include <sstream>
#include <maya/MFnStringArrayData.h>
#include <maya/MFileObject.h>

#ifdef _WIN32
    #include <platform/win32/dirent.h>
    #define PATHSEP ';'
    #define DIRSEP "\\"
    #define LIBEXT MString(".dll")
#else
    #include <sys/types.h>
    #include <dirent.h>
    #include <dlfcn.h>

    #define PATHSEP ':'
    #define DIRSEP "/"

#endif

union DJB2HashUnion{
    unsigned int hash;
    int hashInt;
};

int DJB2Hash(unsigned char *str)
{
    DJB2HashUnion hashUnion;
    hashUnion.hash = 5381;
    int c;

    while ((c = *str++))
        hashUnion.hash = ((hashUnion.hash << 5) + hashUnion.hash) + c; /* hash * 33 + c */

    return hashUnion.hashInt;
}

AtNode*  CABCViewerTranslator::CreateArnoldNodes()
{
    return AddArnoldNode("procedural");
}


void CABCViewerTranslator::ProcessRenderFlagsCustom(AtNode* node)
{
    AiNodeSetByte(node, "visibility", ComputeVisibility());

    MPlug plug;

    if(m_DagNode.findPlug("aiSelfShadows").asBool() == false)
          ProcessParameter(node, "self_shadows", AI_TYPE_BOOLEAN, "aiSelfShadows");

    if(m_DagNode.findPlug("aiOpaque").asBool() == false)
          ProcessParameter(node, "opaque", AI_TYPE_BOOLEAN, "aiOpaque");

        if(m_DagNode.findPlug("aiMatte").asBool() == true)
          ProcessParameter(node, "matte", AI_TYPE_BOOLEAN, "aiMatte");

    if(m_DagNode.findPlug("receiveShadows").asBool() == false)
          ProcessParameter(node, "receive_shadows", AI_TYPE_BOOLEAN, "receiveShadows");
    MStatus status;

    plug = FindMayaPlug("aiSssSetname", &status);
    if (status && !plug.isNull())
    {
        MString setname = plug.asString();
        if (setname != "")
        {
            AiNodeDeclareConstant(node, "sss_setname", AI_TYPE_STRING);
            AiNodeSetStr(node, "sss_setname", setname.asChar());
        }
    }

    ExportTraceSets(node, FindMayaPlug("aiTraceSets"));

    MFnDependencyNode dnode(m_dagPath.node(), &status);
    if (status)
        AiNodeSetInt(node, "id", DJB2Hash((unsigned char*)dnode.name().asChar()));
}

void CABCViewerTranslator::Export(AtNode* procedural)
{
    ExportProcedural(procedural, IsExported());
}


void CABCViewerTranslator::ExportProcedural(AtNode* procedural, bool update)
{
    MStatus stat;
    m_DagNode.setObject(m_dagPath.node());
    
    ExportMatrix(procedural);
    ProcessRenderFlagsCustom(procedural);

    if (!update)
    {
        if (m_DagNode.findPlug("overrideGlobalShader").asBool() == true)
        {

            ExportStandinsShaders(procedural);
        }

        AiNodeSetStr(procedural, "name", m_dagPath.partialPathName().asChar());

        MString procLib = MString("arnoldAlembicProcedural") + LIBEXT ;

        AiNodeSetStr(procedural, "dso", procLib.asChar() );

        bool loadAtInit =  m_DagNode.findPlug("loadAtInit").asBool();
        AiNodeSetBool(procedural, "load_at_init", loadAtInit);

        MString abcfile = m_DagNode.findPlug("cacheFileName").asString();

        MFileObject fileObject;
        fileObject.setRawFullName(abcfile.expandFilePath());
        fileObject.setResolveMethod(MFileObject::kInputFile);
        abcfile = fileObject.resolvedFullName();


        //MTime timeEvaluation = MTime(GetExportFrame(), MTime::uiUnit());
        //MDGContext context = MDGContext (timeEvaluation);

        MTime time = m_DagNode.findPlug("time").asMTime() + m_DagNode.findPlug("timeOffset").asMTime();

        MPlug objectPath = m_DagNode.findPlug("cacheGeomPath");

        MPlug shaders = m_DagNode.findPlug("shaders", &stat);
        if(stat == MS::kSuccess)
        {
            for (unsigned int i=0;i<shaders.numElements();++i)
            {
                MPlug plug = shaders.elementByPhysicalIndex(i, &stat);
                if(stat == MS::kSuccess)
                {
                    MPlugArray connections;
                    plug.connectedTo(connections, true, false, &stat);
                    if(stat == MS::kSuccess)
                        for (unsigned int k=0; k<connections.length(); ++k)
                        {
                            MPlug sgPlug = connections[k];
                            if (sgPlug.node().apiType() == MFn::kShadingEngine || sgPlug.node().apiType() == MFn::kDisplacementShader)
                            {
                                ExportConnectedNode(sgPlug);
                            }
                        }
                }

            }
        }


        MPlug jsonFile = m_DagNode.findPlug("jsonFile");
        MPlug secondaryJsonFile = m_DagNode.findPlug("secondaryJsonFile");
        MPlug shadersNamespace = m_DagNode.findPlug("shadersNamespace");
        MPlug shadersAttribute = m_DagNode.findPlug("shadersAttribute");
        MPlug abcShaders = m_DagNode.findPlug("abcShaders");
        MPlug uvsArchive = m_DagNode.findPlug("uvsArchive");
        MPlug shadersAssignation = m_DagNode.findPlug("shadersAssignation");
        MPlug attributes = m_DagNode.findPlug("attributes");
        MPlug displacementsAssignation = m_DagNode.findPlug("displacementsAssignation");
        MPlug layersOverride = m_DagNode.findPlug("layersOverride");
        
        
        bool skipJsonFile = m_DagNode.findPlug("skipJsonFile").asBool();
        bool skipShaders = m_DagNode.findPlug("skipShaders").asBool();
        bool skipAttributes = m_DagNode.findPlug("skipAttributes").asBool();
        bool skipLayers = m_DagNode.findPlug("skipLayers").asBool();
        bool skipDisplacements = m_DagNode.findPlug("skipDisplacements").asBool();

        if(skipJsonFile)
        {
            AiNodeDeclare(procedural, "skipJsonFile", "constant BOOL");
            AiNodeSetBool(procedural, "skipJsonFile", skipJsonFile);
        }

        if(skipShaders)
        {
            AiNodeDeclare(procedural, "skipShaders", "constant BOOL");
            AiNodeSetBool(procedural, "skipShaders", skipShaders);
        }

        if(skipAttributes)
        {
            AiNodeDeclare(procedural, "skipAttributes", "constant BOOL");
            AiNodeSetBool(procedural, "skipAttributes", skipAttributes);
        }

        if(skipLayers)
        {
            AiNodeDeclare(procedural, "skipLayers", "constant BOOL");
            AiNodeSetBool(procedural, "skipLayers", skipLayers);
        }

        if(skipDisplacements)
        {
            AiNodeDeclare(procedural, "skipDisplacements", "constant BOOL");
            AiNodeSetBool(procedural, "skipDisplacements", skipDisplacements);
        }

        if(abcShaders.asString() != "")
        {
            MFileObject AbcShadersObject; 
            AbcShadersObject.setRawFullName(abcShaders.asString().expandFilePath());
            AbcShadersObject.setResolveMethod(MFileObject::kInputFile);
            MString  AbcShadersFile = AbcShadersObject.resolvedFullName();

            AiNodeDeclare(procedural, "abcShaders", "constant STRING");
            AiNodeSetStr(procedural, "abcShaders", AbcShadersFile.asChar());
        }

        if(uvsArchive.asString() != "")
        {
            MFileObject UVsfileObject;
            UVsfileObject.setRawFullName(uvsArchive.asString().expandFilePath());
            UVsfileObject.setResolveMethod(MFileObject::kInputFile);
            MString UvsFile = UVsfileObject.resolvedFullName();

            AiNodeDeclare(procedural, "uvsArchive", "constant STRING");
            AiNodeSetStr(procedural, "uvsArchive",UvsFile.asChar());
        }

        if(jsonFile.asString() != "")
        {
            MFileObject JSONfileObject;
            JSONfileObject.setRawFullName(jsonFile.asString().expandFilePath());
            JSONfileObject.setResolveMethod(MFileObject::kInputFile);
            MString JSONfile = JSONfileObject.resolvedFullName();

            AiNodeDeclare(procedural, "jsonFile", "constant STRING");
            AiNodeSetStr(procedural, "jsonFile", JSONfile.asChar());
        }

        if(secondaryJsonFile.asString() != "")
        {
            MFileObject JSONfileObject;
            JSONfileObject.setRawFullName(secondaryJsonFile.asString().expandFilePath());
            JSONfileObject.setResolveMethod(MFileObject::kInputFile);
            MString JSONfile = JSONfileObject.resolvedFullName();

            AiNodeDeclare(procedural, "secondaryJsonFile", "constant STRING");
            AiNodeSetStr(procedural, "secondaryJsonFile", JSONfile.asChar());
        }

        if(shadersNamespace.asString() != "")
        {
            AiNodeDeclare(procedural, "shadersNamespace", "constant STRING");
            AiNodeSetStr(procedural, "shadersNamespace", shadersNamespace.asString().asChar());
        }

        if(shadersAttribute.asString() != "")
        {
            AiNodeDeclare(procedural, "shadersAttribute", "constant STRING");
            AiNodeSetStr(procedural, "shadersAttribute", shadersAttribute.asString().asChar());
        }

        if(shadersAssignation.asString() != "")
        {
            AiNodeDeclare(procedural, "shadersAssignation", "constant STRING");
            AiNodeSetStr(procedural, "shadersAssignation", shadersAssignation.asString().asChar());
        }

        if(attributes.asString() != "")
        {
            AiNodeDeclare(procedural, "attributes", "constant STRING");
            AiNodeSetStr(procedural, "attributes", attributes.asString().asChar());
        }

        if(displacementsAssignation.asString() != "")
        {
            AiNodeDeclare(procedural, "displacementsAssignation", "constant STRING");
            AiNodeSetStr(procedural, "displacementsAssignation", displacementsAssignation.asString().asChar());
        }

        if(layersOverride.asString() != "")
        {
            AiNodeDeclare(procedural, "layersOverride", "constant STRING");
            AiNodeSetStr(procedural, "layersOverride", layersOverride.asString().asChar());
        }

        std::string objectPathStr = objectPath.asString().asChar();


        objectPathStr = pystring::replace(objectPathStr, "|", "/");

        if(objectPathStr == "")
            objectPathStr = "/";

        float fps = 25.0f;

        static const MTime sec(1.0, MTime::kSeconds);
        fps = sec.as(MTime::uiUnit());

        MString data;
        data += "-filename " +  abcfile + " -objectpath " + MString(objectPathStr.c_str()) + " -nameprefix " + m_dagPath.partialPathName() + " -frame " + time.as(time.unit()) + " -fps " + fps;

        AiNodeSetStr(procedural, "data", data.expandEnvironmentVariablesAndTilde().asChar());

        ExportBoundingBox(procedural);
        /*
        if (!AiNodeLookUpUserParameter(procedural, "allow_updates"))
        {
            AiNodeDeclare(procedural, "allow_updates", "constant BOOL");
        }
        AiNodeSetBool(procedural, "allow_updates", true); // do we need a security valve here ? like a new parameter to control that ?
        */
    }
}



void CABCViewerTranslator::ExportShaders()
{
  ExportStandinsShaders(GetArnoldNode());
}


void CABCViewerTranslator::ExportStandinsShaders(AtNode* procedural)
{

    int instanceNum = m_dagPath.isInstanced() ? m_dagPath.instanceNumber() : 0;

    std::vector<AtNode*> meshShaders;

    MPlug shadingGroupPlug = GetNodeShadingGroup(m_dagPath.node(), instanceNum);
    if (!shadingGroupPlug.isNull())
    {

        AtNode *shader = ExportConnectedNode(shadingGroupPlug);
        if (shader != NULL)
        {
            AiNodeSetPtr(procedural, "shader", shader);
            meshShaders.push_back(shader);
        }
        else
        {
            AiMsgWarning("[mtoa] [translator %s] ShadingGroup %s has no surfaceShader input",
                    GetTranslatorName().asChar(), MFnDependencyNode(shadingGroupPlug.node()).name().asChar());
            /*AiMsgWarning("[mtoa] ShadingGroup %s has no surfaceShader input.",
                    fnDGNode.name().asChar());*/
            AiNodeSetPtr(procedural, "shader", NULL);
        }
    }
}

void CABCViewerTranslator::ExportMotion(AtNode* anode)
{
   // Check if motionblur is enabled and early out if it's not.
   if (!IsMotionBlurEnabled()) return;

    ExportMatrix(anode);
}

void CABCViewerTranslator::ExportBoundingBox(AtNode* procedural)
{
    MBoundingBox boundingBox = m_DagNode.boundingBox();
    MPoint bbMin = boundingBox.min();
    MPoint bbMax = boundingBox.max();

    float minCoords[4];
    float maxCoords[4];

    bbMin.get(minCoords);
    bbMax.get(maxCoords);

    AiNodeSetPnt(procedural, "min", minCoords[0], minCoords[1], minCoords[2]);
    AiNodeSetPnt(procedural, "max", maxCoords[0], maxCoords[1], maxCoords[2]);
}



void CABCViewerTranslator::NodeInitializer(CAbTranslator context)
{
    CExtensionAttrHelper helper(context.maya, "procedural");
    CShapeTranslator::MakeCommonAttributes(helper);

    CAttrData data;

    data.defaultValue.BOOL = false;
    data.name = "overrideGlobalShader";
    data.shortName = "ogs";
    helper.MakeInputBoolean(data) ;

    data.defaultValue.BOOL = true;
    data.name = "loadAtInit";
    data.shortName = "lai";
    data.channelBox = true;
    helper.MakeInputBoolean(data);

}
