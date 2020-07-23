#include <cmath>
#include <limits>
#include "G3D/G3D.h"
#include "gtest/gtest.h"


TEST(G3DMath, NumericLimits) {
    ASSERT_EQ(std::fpclassify(G3D::inf()), FP_INFINITE);
    ASSERT_EQ(std::fpclassify(G3D::finf()), FP_INFINITE);
    ASSERT_FALSE(G3D::inf() < G3D::inf());
    ASSERT_FALSE(G3D::finf() < G3D::finf());
}