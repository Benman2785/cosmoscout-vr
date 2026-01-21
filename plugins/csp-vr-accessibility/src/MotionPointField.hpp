////////////////////////////////////////////////////////////////////////////////////////////////////
//                               This file is part of CosmoScout VR                               //
////////////////////////////////////////////////////////////////////////////////////////////////////

// SPDX-FileCopyrightText: German Aerospace Center (DLR)
// SPDX-License-Identifier: MIT

#ifndef CSP_VRACCESSIBILITY_MOTIONPOINTFIELD_HPP
#define CSP_VRACCESSIBILITY_MOTIONPOINTFIELD_HPP

#include "Plugin.hpp"

#include <VistaKernel/GraphicsManager/VistaOpenGLDraw.h>
#include <VistaKernel/GraphicsManager/VistaOpenGLNode.h>

#include <glm/glm.hpp>

#include <memory>
#include <vector>

namespace cs::core {
class SolarSystem;
}

namespace csp::vraccessibility {

////////////////////////////////////////////////////////////////////////////////////////////////////

class MotionPointField : public IVistaOpenGLDraw {

 public:
  MotionPointField(
      std::shared_ptr<cs::core::SolarSystem> solarSystem, Plugin::Settings::MotionPoints& settings);

  ~MotionPointField() override;

  MotionPointField(MotionPointField const&)            = delete;
  MotionPointField& operator=(MotionPointField const&) = delete;

  bool Do() override;
  bool GetBoundingBox(VistaBoundingBox& bb) override;

 private:
  void      generatePoints();
  void      updatePoints();
  glm::vec3 randomPointInSphere();

  // -----------------------------------------------------------------------------------------------

  std::shared_ptr<cs::core::SolarSystem> mSolarSystem;
  Plugin::Settings::MotionPoints&        mSettings;

  std::vector<glm::vec3> mPoints;

  int   mLastPointCount = -1;
  float mLastRadius     = -1.0f;

  // OpenGL
  VistaBufferObject mVBO;
  VistaVertexArrayObject  mVAO;
  VistaGLSLShader        mShader;

  struct {
    int modelView  = -1;
    int projection = -1;
    int color      = -1;
    int pointSize  = -1;
  } mUniforms;

  std::unique_ptr<VistaTransformNode> mRoot;
  std::unique_ptr<VistaOpenGLNode>    mGLNode;

  static const char* VERT_SHADER;
  static const char* FRAG_SHADER;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace csp::vraccessibility

#endif // CSP_VRACCESSIBILITY_MOTIONPOINTFIELD_HPP
