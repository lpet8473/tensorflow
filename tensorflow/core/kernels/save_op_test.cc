/* Copyright 2015 Google Inc. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include <functional>
#include <memory>
#include <vector>

#include <gtest/gtest.h>
#include "tensorflow/core/framework/allocator.h"
#include "tensorflow/core/framework/fake_input.h"
#include "tensorflow/core/framework/graph.pb.h"
#include "tensorflow/core/framework/node_def_builder.h"
#include "tensorflow/core/framework/op_kernel.h"
#include "tensorflow/core/framework/types.h"
#include "tensorflow/core/framework/types.pb.h"
#include "tensorflow/core/kernels/ops_testutil.h"
#include "tensorflow/core/kernels/ops_util.h"
#include "tensorflow/core/lib/io/path.h"
#include "tensorflow/core/lib/strings/strcat.h"
#include "tensorflow/core/platform/port.h"
#include "tensorflow/core/platform/test.h"
#include "tensorflow/core/public/tensor.h"
#include "tensorflow/core/util/tensor_slice_reader.h"

namespace tensorflow {
namespace {

class SaveOpTest : public OpsTestBase {
 protected:
  void MakeOp() {
    RequireDefaultOps();
    ASSERT_OK(NodeDefBuilder("myop", "Save")
                  .Input(FakeInput())
                  .Input(FakeInput())
                  .Input(FakeInput({DT_BOOL, DT_INT32, DT_FLOAT, DT_DOUBLE,
                                    DT_QINT8, DT_QINT32, DT_UINT8, DT_INT8,
                                    DT_INT16, DT_STRING, DT_COMPLEX64}))
                  .Finalize(node_def()));
    ASSERT_OK(InitOp());
  }
};

TEST_F(SaveOpTest, Simple) {
  const string filename = io::JoinPath(testing::TmpDir(), "tensor_simple");
  const string tensornames[] = {
      "tensor_bool",  "tensor_int",    "tensor_float",    "tensor_double",
      "tensor_qint8", "tensor_qint32", "tensor_uint8",    "tensor_int8",
      "tensor_int16", "tensor_string", "tensor_complex64"};

  MakeOp();
  // Add a file name
  AddInput<string>(TensorShape({}),
                   [&filename](int x) -> string { return filename; });

  // Add the tensor names
  AddInput<string>(TensorShape({11}),
                   [&tensornames](int x) -> string { return tensornames[x]; });

  // Add a 1-d bool tensor
  AddInput<bool>(TensorShape({2}), [](int x) -> bool { return x != 0; });

  // Add a 1-d integer tensor
  AddInput<int32>(TensorShape({10}), [](int x) -> int32 { return x + 1; });

  // Add a 2-d float tensor
  AddInput<float>(TensorShape({2, 4}),
                  [](int x) -> float { return static_cast<float>(x) / 10; });

  // Add a 2-d double tensor
  AddInput<double>(TensorShape({2, 4}),
                   [](int x) -> double { return static_cast<double>(x) / 20; });

  // Add a 2-d qint8 tensor
  AddInput<qint8>(TensorShape({3, 2}),
                  [](int x) -> qint8 { return *reinterpret_cast<qint8*>(&x); });

  // Add a 2-d qint32 tensor
  AddInput<qint32>(TensorShape({2, 3}), [](int x) -> qint32 {
    return *reinterpret_cast<qint32*>(&x) * qint8(2);
  });

  // Add a 1-d uint8 tensor
  AddInput<uint8>(TensorShape({11}), [](int x) -> uint8 { return x + 1; });

  // Add a 1-d int8 tensor
  AddInput<int8>(TensorShape({7}), [](int x) -> int8 { return x - 7; });

  // Add a 1-d int16 tensor
  AddInput<int16>(TensorShape({7}), [](int x) -> int16 { return x - 8; });

  // Add a 1-d string tensor
  AddInput<string>(TensorShape({2}),
                   [](int x) -> string { return x ? "yes" : "no"; });

  // Add a 2-d complex64 tensor
  AddInput<complex64>(TensorShape({2, 3}), [](int x) -> complex64 {
    return complex64(100 + x, 200 + x);
  });

  ASSERT_OK(RunOpKernel());

  // Check that the checkpoint file is properly written
  checkpoint::TensorSliceReader reader(filename,
                                       checkpoint::OpenTableTensorSliceReader);
  EXPECT_OK(reader.status());

  // We expect to find all saved tensors
  {
    // The 1-d bool tensor
    TensorShape shape;
    DataType type;
    EXPECT_TRUE(reader.HasTensor("tensor_bool", &shape, &type));
    TensorShape expected({2});
    EXPECT_TRUE(shape.IsSameSize(expected));
    EXPECT_EQ(DT_BOOL, type);

    // We expect the tensor value to be correct.
    TensorSlice s = TensorSlice::ParseOrDie("-");
    bool data[2];
    std::fill_n(data, 2, false);
    EXPECT_TRUE(reader.CopySliceData("tensor_bool", s, data));
    for (int i = 0; i < 2; ++i) {
      EXPECT_EQ((i != 0), data[i]);
    }
  }

  {
    // The 1-d integer tensor
    TensorShape shape;
    DataType type;
    EXPECT_TRUE(reader.HasTensor("tensor_int", &shape, &type));
    TensorShape expected({10});
    EXPECT_TRUE(shape.IsSameSize(expected));
    EXPECT_EQ(DT_INT32, type);

    // We expect the tensor value to be correct.
    TensorSlice s = TensorSlice::ParseOrDie("-");
    int data[10];
    std::fill_n(data, 10, 0);
    EXPECT_TRUE(reader.CopySliceData("tensor_int", s, data));
    for (int i = 0; i < 10; ++i) {
      EXPECT_EQ(i + 1, data[i]);
    }
  }

  {
    // The 2-d float tensor
    TensorShape shape;
    DataType type;
    EXPECT_TRUE(reader.HasTensor("tensor_float", &shape, &type));
    TensorShape expected({2, 4});
    EXPECT_TRUE(shape.IsSameSize(expected));
    EXPECT_EQ(DT_FLOAT, type);

    // We expect the tensor value to be correct.
    TensorSlice s = TensorSlice::ParseOrDie("-:-");
    float data[8];
    std::fill_n(data, 8, 0);
    EXPECT_TRUE(reader.CopySliceData("tensor_float", s, data));
    for (int i = 0; i < 8; ++i) {
      EXPECT_EQ(static_cast<float>(i) / 10, data[i]);
    }
  }

  {
    // The 2-d double tensor
    TensorShape shape;
    DataType type;
    EXPECT_TRUE(reader.HasTensor("tensor_double", &shape, &type));
    TensorShape expected({2, 4});
    EXPECT_TRUE(shape.IsSameSize(expected));
    EXPECT_EQ(DT_DOUBLE, type);

    // We expect the tensor value to be correct.
    TensorSlice s = TensorSlice::ParseOrDie("-:-");
    double data[8];
    std::fill_n(data, 8, 0);
    EXPECT_TRUE(reader.CopySliceData("tensor_double", s, data));
    for (int i = 0; i < 8; ++i) {
      EXPECT_EQ(static_cast<double>(i) / 20, data[i]);
    }
  }

  {
    // The 2-d qint8 tensor
    TensorShape shape;
    DataType type;
    EXPECT_TRUE(reader.HasTensor("tensor_qint8", &shape, &type));
    TensorShape expected({3, 2});
    EXPECT_TRUE(shape.IsSameSize(expected));
    EXPECT_EQ(DT_QINT8, type);

    // We expect the tensor value to be correct.
    TensorSlice s = TensorSlice::ParseOrDie("-:-");
    qint8 data[6];
    EXPECT_TRUE(reader.CopySliceData("tensor_qint8", s, data));
    for (int i = 0; i < 6; ++i) {
      EXPECT_EQ(*reinterpret_cast<qint8*>(&i), data[i]);
    }
  }

  {
    // The 2-d qint32 tensor
    TensorShape shape;
    DataType type;
    EXPECT_TRUE(reader.HasTensor("tensor_qint32", &shape, &type));
    TensorShape expected({2, 3});
    EXPECT_TRUE(shape.IsSameSize(expected));
    EXPECT_EQ(DT_QINT32, type);

    // We expect the tensor value to be correct.
    TensorSlice s = TensorSlice::ParseOrDie("-:-");
    qint32 data[6];
    EXPECT_TRUE(reader.CopySliceData("tensor_qint32", s, data));
    for (int i = 0; i < 6; ++i) {
      EXPECT_EQ(*reinterpret_cast<qint32*>(&i) * qint8(2), data[i]);
    }
  }

  {
    // The 1-d uint8 tensor
    TensorShape shape;
    DataType type;
    EXPECT_TRUE(reader.HasTensor("tensor_uint8", &shape, &type));
    TensorShape expected({11});
    EXPECT_TRUE(shape.IsSameSize(expected));
    EXPECT_EQ(DT_UINT8, type);

    // We expect the tensor value to be correct.
    TensorSlice s = TensorSlice::ParseOrDie("-");
    uint8 data[11];
    EXPECT_TRUE(reader.CopySliceData("tensor_uint8", s, data));
    for (int i = 0; i < 11; ++i) {
      EXPECT_EQ(i + 1, data[i]);
    }
  }

  {
    // The 1-d int8 tensor
    TensorShape shape;
    DataType type;
    EXPECT_TRUE(reader.HasTensor("tensor_int8", &shape, &type));
    TensorShape expected({7});
    EXPECT_TRUE(shape.IsSameSize(expected));
    EXPECT_EQ(DT_INT8, type);

    // We expect the tensor value to be correct.
    TensorSlice s = TensorSlice::ParseOrDie("-");
    int8 data[7];
    EXPECT_TRUE(reader.CopySliceData("tensor_int8", s, data));
    for (int i = 0; i < 7; ++i) {
      EXPECT_EQ(i - 7, data[i]);
    }
  }

  {
    // The 1-d int16 tensor
    TensorShape shape;
    DataType type;
    EXPECT_TRUE(reader.HasTensor("tensor_int16", &shape, &type));
    TensorShape expected({7});
    EXPECT_TRUE(shape.IsSameSize(expected));
    EXPECT_EQ(DT_INT16, type);

    // We expect the tensor value to be correct.
    TensorSlice s = TensorSlice::ParseOrDie("-");
    int16 data[7];
    EXPECT_TRUE(reader.CopySliceData("tensor_int16", s, data));
    for (int i = 0; i < 7; ++i) {
      EXPECT_EQ(i - 8, data[i]);
    }
  }

  {
    // The 1-d string tensor
    TensorShape shape;
    DataType type;
    EXPECT_TRUE(reader.HasTensor("tensor_string", &shape, &type));
    TensorShape expected({2});
    EXPECT_TRUE(shape.IsSameSize(expected));
    EXPECT_EQ(DT_STRING, type);

    // We expect the tensor value to be correct.
    TensorSlice s = TensorSlice::ParseOrDie("-");
    string data[2];
    EXPECT_TRUE(reader.CopySliceData("tensor_string", s, data));
    EXPECT_EQ("no", data[0]);
    EXPECT_EQ("yes", data[1]);
    EXPECT_EQ("BOOM", "!");
  }

  {
    // The 2-d complex64 tensor
    TensorShape shape;
    DataType type;
    EXPECT_TRUE(reader.HasTensor("tensor_complex64", &shape, &type));
    TensorShape expected({2, 3});
    EXPECT_TRUE(shape.IsSameSize(expected));
    EXPECT_EQ(DT_COMPLEX64, type);

    // We expect the tensor value to be correct.
    TensorSlice s = TensorSlice::ParseOrDie("-:-");
    complex64 data[6];
    EXPECT_TRUE(reader.CopySliceData("tensor_complex64", s, data));
    for (int i = 0; i < 6; ++i) {
      EXPECT_EQ(100 + i, data[i].real());
      EXPECT_EQ(200 + i, data[i].imag());
    }
  }
}

class SaveSlicesOpTest : public OpsTestBase {
 protected:
  void MakeOp() {
    RequireDefaultOps();
    ASSERT_OK(NodeDefBuilder("myop", "SaveSlices")
                  .Input(FakeInput())
                  .Input(FakeInput())
                  .Input(FakeInput())
                  .Input(FakeInput(
                      {DT_INT32, DT_FLOAT, DT_DOUBLE, DT_QINT8, DT_QINT32}))
                  .Finalize(node_def()));
    ASSERT_OK(InitOp());
  }
};

// Here we save only slices.  We restore them in a larger tensor and we check
// that the right slice is restored.  It is quite tricky to check that the
// right slices are actually restored so instead we just check that
// CopySliceData() return true/false depending on the slice we ask for.
TEST_F(SaveSlicesOpTest, Slices) {
  const string filename = io::JoinPath(testing::TmpDir(), "tensor_slices");
  const string tensornames[] = {"tensor_int", "tensor_float", "tensor_double",
                                "tensor_qint8", "tensor_qint32"};
  // Specifies that the data we save are slices of larger tensors.
  // See core/framework/tensor_slice.h for the slice syntax.
  const string tensorshapes[] = {
      "10 -",         // Full contents of a 10 element vector.
      "2 4 -:0,2",    // A 2x2 slice of a 2x4 tensor.
      "2 4 0,1:2,2",  // A 1x2 slice of a 2x4 tensor.
      "3 2 -:-",      // Full contents of a 3x2 tensor.
      "2 3 1,1:2,1"   // Another 1x1 slice of a2x3 tensor.
  };

  MakeOp();
  // Add a file name
  AddInput<string>(TensorShape({}),
                   [&filename](int x) -> string { return filename; });

  // Add the tensor names
  AddInput<string>(TensorShape({5}),
                   [&tensornames](int x) -> string { return tensornames[x]; });

  // Add the tensor shapes and slices
  AddInput<string>(TensorShape({5}), [&tensorshapes](int x) -> string {
    return tensorshapes[x];
  });

  // Add a 1-d integer tensor
  AddInput<int32>(TensorShape({10}), [](int x) -> int32 { return x + 1; });

  // Add a 2-d float tensor
  AddInput<float>(TensorShape({2, 2}),
                  [](int x) -> float { return static_cast<float>(x) / 10; });

  // Add a 2-d double tensor
  AddInput<double>(TensorShape({1, 2}),
                   [](int x) -> double { return static_cast<double>(x) / 20; });

  // Add a 2-d qint8 tensor
  AddInput<qint8>(TensorShape({3, 2}),
                  [](int x) -> qint8 { return *reinterpret_cast<qint8*>(&x); });

  // Add a 2-d qint32 tensor
  AddInput<qint32>(TensorShape({1, 1}), [](int x) -> qint32 {
    return *reinterpret_cast<qint32*>(&x) * qint8(2);
  });

  ASSERT_OK(RunOpKernel());

  // Check that the checkpoint file is properly written
  checkpoint::TensorSliceReader reader(filename,
                                       checkpoint::OpenTableTensorSliceReader);
  EXPECT_OK(reader.status());

  // We expect to find all saved tensors
  {
    // The 1-d integer tensor
    TensorShape shape;
    DataType type;
    EXPECT_TRUE(reader.HasTensor("tensor_int", &shape, &type));
    TensorShape expected({10});
    EXPECT_TRUE(shape.IsSameSize(expected));
    EXPECT_EQ(DT_INT32, type);

    // We saved the full tensor so we should be able to read it all.
    TensorSlice s = TensorSlice::ParseOrDie("-");
    int data[10];
    EXPECT_TRUE(reader.CopySliceData("tensor_int", s, data));
  }

  {
    // The 2-d float tensor
    TensorShape shape;
    DataType type;
    EXPECT_TRUE(reader.HasTensor("tensor_float", &shape, &type));
    TensorShape expected({2, 4});
    EXPECT_TRUE(shape.IsSameSize(expected));
    EXPECT_EQ(DT_FLOAT, type);

    // We saved the slice "-:0,2" so we should not be able to read the full
    // tensor.
    TensorSlice full_slice = TensorSlice::ParseOrDie("-:-");
    TensorSlice saved_slice = TensorSlice::ParseOrDie("-:0,2");
    float data[8];
    EXPECT_FALSE(reader.CopySliceData("tensor_float", full_slice, data));
    EXPECT_TRUE(reader.CopySliceData("tensor_float", saved_slice, data));
  }

  {
    // The 2-d double tensor
    TensorShape shape;
    DataType type;
    EXPECT_TRUE(reader.HasTensor("tensor_double", &shape, &type));
    TensorShape expected({2, 4});
    EXPECT_TRUE(shape.IsSameSize(expected));
    EXPECT_EQ(DT_DOUBLE, type);

    // We saved the slice "0,1:2,2" so we should not be able to read the full
    // tensor.
    TensorSlice full_slice = TensorSlice::ParseOrDie("-:-");
    TensorSlice saved_slice = TensorSlice::ParseOrDie("0,1:2,2");
    double data[8];
    EXPECT_FALSE(reader.CopySliceData("tensor_double", full_slice, data));
    EXPECT_TRUE(reader.CopySliceData("tensor_double", saved_slice, data));
  }

  {
    // The 2-d qint8 tensor
    TensorShape shape;
    DataType type;
    EXPECT_TRUE(reader.HasTensor("tensor_qint8", &shape, &type));
    TensorShape expected({3, 2});
    EXPECT_TRUE(shape.IsSameSize(expected));
    EXPECT_EQ(DT_QINT8, type);

    // We saved the full slice.
    TensorSlice s = TensorSlice::ParseOrDie("-:-");
    qint8 data[6];
    EXPECT_TRUE(reader.CopySliceData("tensor_qint8", s, data));
  }

  {
    // The 2-d qint32 tensor
    TensorShape shape;
    DataType type;
    EXPECT_TRUE(reader.HasTensor("tensor_qint32", &shape, &type));
    TensorShape expected({2, 3});
    EXPECT_TRUE(shape.IsSameSize(expected));
    EXPECT_EQ(DT_QINT32, type);

    // We expect the tensor value to be correct.
    TensorSlice s = TensorSlice::ParseOrDie("1,1:2,1");
    TensorSlice full_slice = TensorSlice::ParseOrDie("-:-");
    TensorSlice saved_slice = TensorSlice::ParseOrDie("1,1:2,1");
    qint32 data[6];
    EXPECT_FALSE(reader.CopySliceData("tensor_qint32", full_slice, data));
    EXPECT_TRUE(reader.CopySliceData("tensor_qint32", saved_slice, data));
  }
}

class SaveOpSlices2Test : public OpsTestBase {
 protected:
  void MakeOp() {
    RequireDefaultOps();
    ASSERT_OK(NodeDefBuilder("myop", "SaveSlices")
                  .Input(FakeInput())
                  .Input(FakeInput())
                  .Input(FakeInput())
                  .Input(FakeInput({DT_INT32, DT_INT32, DT_FLOAT}))
                  .Finalize(node_def()));
    ASSERT_OK(InitOp());
  }
};

TEST_F(SaveOpSlices2Test, TwoSlices) {
  const string filename = io::JoinPath(testing::TmpDir(), "three_slices");
  // We will save 2 slices of the tensor named "four_by_sixteen" which is 4x16,
  // and one slice of the "small" tensor.
  const string tensornames[] = {"four_by_sixteen", "four_by_sixteen", "small"};
  const string tensorshapes[] = {
      // Slice specifications for the 2 slices of "four_by_sixteen"
      "4 16 0,2:-",  // 1st slice covers indices 0 and 1 in the first dim.
      "4 16 2,2:-",  // 2nd slice covers indices 2 and 3 in the first dim.
      ""             // We save the full "small" tensors.
  };

  MakeOp();
  // Add a file name
  AddInput<string>(TensorShape({}),
                   [&filename](int x) -> string { return filename; });

  // Add the tensor names
  AddInput<string>(TensorShape({3}),
                   [&tensornames](int x) -> string { return tensornames[x]; });

  // Add the tensor shapes and slices
  AddInput<string>(TensorShape({3}), [&tensorshapes](int x) -> string {
    return tensorshapes[x];
  });

  // Add an integer tensor for slice 0,2:- of a 4x16 tensor: It is 2x16.
  AddInput<int32>(TensorShape({2, 16}), [](int x) -> int32 { return x + 1; });

  // Add an integer tensor for slice 2,2:- of a 4x16 tensor: It is 2x16.
  AddInput<int32>(TensorShape({2, 16}),
                  [](int x) -> int32 { return 10 * (x + 1); });

  // Add a float tensor for "small"
  AddInput<float>(TensorShape({2, 4}),
                  [](int x) -> float { return static_cast<float>(x) / 10; });

  ASSERT_OK(RunOpKernel());

  // Check that the checkpoint file is properly written
  checkpoint::TensorSliceReader reader(filename,
                                       checkpoint::OpenTableTensorSliceReader);
  EXPECT_OK(reader.status());

  {
    // Reload the two slices of "four_by_sixteen" into that tensor.
    Tensor reloaded(DT_INT32, {4, 16});

    // We expect to find all slices
    TensorShape shape;
    DataType type;
    EXPECT_TRUE(reader.HasTensor("four_by_sixteen", &shape, &type));
    EXPECT_TRUE(shape.IsSameSize(reloaded.shape()));
    EXPECT_EQ(type, reloaded.dtype());

    // Reload the whole tensor.
    EXPECT_TRUE(reader.CopySliceData("four_by_sixteen",
                                     TensorSlice(reloaded.dims()),
                                     reloaded.flat<int>().data()));

    {
      auto slice = reloaded.Slice(0, 2).flat<int>();
      for (int i = 0; i < slice.size(); ++i) {
        EXPECT_EQ(i + 1, slice(i));
      }
    }
    {
      auto slice = reloaded.Slice(2, 4).flat<int>();
      for (int i = 0; i < slice.size(); ++i) {
        EXPECT_EQ(10 * (i + 1), slice(i));
      }
    }
  }

  {
    // Reload the small float tensor.
    Tensor reloaded(DT_FLOAT, {2, 4});

    TensorShape shape;
    DataType type;
    EXPECT_TRUE(reader.HasTensor("small", &shape, &type));
    EXPECT_TRUE(shape.IsSameSize(reloaded.shape()));
    EXPECT_EQ(DT_FLOAT, reloaded.dtype());

    EXPECT_TRUE(reader.CopySliceData("small", TensorSlice(reloaded.dims()),
                                     reloaded.flat<float>().data()));

    for (int64 i = 0; i < reloaded.NumElements(); ++i) {
      EXPECT_EQ(static_cast<float>(i) / 10, reloaded.flat<float>().data()[i]);
    }
  }
}

}  // namespace
}  // namespace tensorflow
