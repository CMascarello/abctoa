# Alembic Holder
# Copyright (c) 2014, Gaël Honorez, All rights reserved.
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 3.0 of the License, or (at your option) any later version.
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# Lesser General Public License for more details.
# You should have received a copy of the GNU Lesser General Public
# License along with this library.

import maya.cmds as cmds
import shaderManager

def alembicShaderManager():
    reload(shaderManager)
    shaderManager.manager().show()

def createAlembicHolder():
    x = cmds.createNode('alembicHolder', n="AlembicHolderShape")
    cmds.setAttr("%s.overrideLevelOfDetail" % x, 1)
    cmds.setAttr("%s.overrideVisibility" % x, 1) 
    cmds.setAttr("%s.visibleInRefractions" % x, 1)
    cmds.setAttr("%s.visibleInReflections" % x, 1)
    cmds.connectAttr("time1.outTime", "%s.time" % x)


def registerAlembicHolder():
    if not cmds.about(b=1):
        cmds.menu('AlembicHolderMenu', label='Alembic Holder', parent='MayaWindow', tearOff=True )
        cmds.menuItem('CreateAlembicHolder', label='Create Holder', parent='AlembicHolderMenu', c=lambda *args: createAlembicHolder())
        cmds.menuItem('AlembicShaderManager', label='Shader Manager', parent='AlembicHolderMenu', c=lambda *args: alembicShaderManager())
