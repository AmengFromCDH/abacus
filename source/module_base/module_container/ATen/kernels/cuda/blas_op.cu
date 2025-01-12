#include <ATen/kernels/blas_op.h>
#include <base/third_party/blas.h>

#include <cuda_runtime.h>
#include <cublas_v2.h>

namespace container {
namespace op {

static cublasHandle_t cublas_handle = nullptr;

void createGpuBlasHandle() {
    if (cublas_handle == nullptr) {
        cublasErrcheckInternal(cublasCreate(&cublas_handle));
    }
}

void destroyGpuBlasHandle() {
    if (cublas_handle != nullptr) {
        cublasErrcheckInternal(cublasDestroy(cublas_handle));
        cublas_handle = nullptr;
    }
}


template <typename T>
struct blas_dot<T, DEVICE_GPU> {
    void operator()(
        const int& n,
        const T* x,
        const int& incx,
        const T* y,
        const int& incy,
        T* result)
    {
        cuBlasConnector::dot(cublas_handle, n, x, incx, y, incy, result);
    }
};

template <typename T>
struct blas_scal<T, DEVICE_GPU> {
    void operator()(
        const int& n,
        const T* alpha,
        T* x,
        const int& incx)
    {
        cuBlasConnector::scal(cublas_handle, n, *alpha, x, incx);
    }
};

template <typename T>
struct blas_axpy<T, DEVICE_GPU> {
    void operator()(
        const int& n,
        const T* alpha,
        const T* x,
        const int& incx,
        T* y,
        const int& incy)
    {
        cuBlasConnector::axpy(cublas_handle, n, *alpha, x, incx, y, incy);
    }
};

template <typename T>
struct blas_gemv<T, DEVICE_GPU> {
    void operator()(
        const char& trans,
        const int& m,
        const int& n,
        const T* alpha,
        const T* A,
        const int& lda,
        const T* x,
        const int& incx,
        const T* beta,
        T* y,
        const int& incy) 
    {
        cuBlasConnector::gemv(cublas_handle, trans, m, n, *alpha, A, lda, x, incx, *beta, y, incy);
    }
};


template <typename T>
struct blas_gemv_batched<T, DEVICE_GPU> {
    void operator()(
        const char& trans,
        const int& m,
        const int& n,
        const T* alpha,
        T** A,
        const int& lda,
        T** x,
        const int& incx,
        const T* beta,
        T** y,
        const int& incy,
        const int& batch_size)
    {
        cuBlasConnector::gemv_batched(cublas_handle, trans, m, n, *alpha, A, lda, x, incx, *beta, y, incy, batch_size);
    }
};


template <typename T>
struct blas_gemv_batched_strided<T, DEVICE_GPU> {
    void operator()(
        const char& trans,
        const int& m,
        const int& n,
        const T* alpha,
        const T* A,
        const int& lda,
        const int64_t& stride_a,
        const T* x,
        const int& incx,
        const int64_t& stride_x,
        const T* beta,
        T* y,
        const int& incy,
        const int64_t& stride_y,
        const int& batch_size)
    {
        cuBlasConnector::gemv_batched_strided(cublas_handle, trans, m, n, *alpha, A, lda, stride_a, x, incx, stride_x, *beta, y, incy, stride_y, batch_size);
    }
};

template <typename T>
struct blas_gemm<T, DEVICE_GPU> {
    void operator()(
        const char& transa,
        const char& transb,
        const int& m,
        const int& n,
        const int& k,
        const T* alpha,
        const T* A,
        const int& lda,
        const T* B,
        const int& ldb,
        const T* beta,
        T* C,
        const int& ldc)
    {
        cuBlasConnector::gemm(cublas_handle, transa, transb, m, n, k, *alpha, A, lda, B, ldb, *beta, C, ldc);
    }
};

template <typename T>
struct blas_gemm_batched<T, DEVICE_GPU> {
    void operator()(
        const char& transa,
        const char& transb,
        const int& m,
        const int& n,
        const int& k,
        const T* alpha,
        T** A,
        const int& lda,
        T** B,
        const int& ldb,
        const T* beta,
        T** C,
        const int& ldc,
        const int& batch_size)
    {
        cuBlasConnector::gemm_batched(cublas_handle, transa, transb, m, n, k, *alpha, A, lda, B, ldb, *beta, C, ldc, batch_size);
    }
};

template <typename T>
struct blas_gemm_batched_strided<T, DEVICE_GPU> {
    void operator()(
        const char& transa,
        const char& transb,
        const int& m,
        const int& n,
        const int& k,
        const T* alpha,
        const T* A,
        const int& lda,
        const int& stride_a,
        const T* B,
        const int& ldb,
        const int& stride_b,
        const T* beta,
        T* C,
        const int& ldc,
        const int& stride_c,
        const int& batch_size)
    {
        cuBlasConnector::gemm_batched_strided(cublas_handle, transa, transb, m, n, k, *alpha, A, lda, stride_a, B, ldb, stride_b, *beta, C, ldc, stride_c, batch_size);
    }
};

// Explicitly instantiate functors for the types of functor registered.
template struct blas_dot<float , DEVICE_GPU>;
template struct blas_dot<double, DEVICE_GPU>;
template struct blas_dot<std::complex<float> , DEVICE_GPU>;
template struct blas_dot<std::complex<double>, DEVICE_GPU>;

template struct blas_scal<float , DEVICE_GPU>;
template struct blas_scal<double, DEVICE_GPU>;
template struct blas_scal<std::complex<float> , DEVICE_GPU>;
template struct blas_scal<std::complex<double>, DEVICE_GPU>;

template struct blas_axpy<float , DEVICE_GPU>;
template struct blas_axpy<double, DEVICE_GPU>;
template struct blas_axpy<std::complex<float> , DEVICE_GPU>;
template struct blas_axpy<std::complex<double>, DEVICE_GPU>;

template struct blas_gemv<float , DEVICE_GPU>;
template struct blas_gemv<double, DEVICE_GPU>;
template struct blas_gemv<std::complex<float >, DEVICE_GPU>;
template struct blas_gemv<std::complex<double>, DEVICE_GPU>;

template struct blas_gemv_batched<float , DEVICE_GPU>;
template struct blas_gemv_batched<double, DEVICE_GPU>;
template struct blas_gemv_batched<std::complex<float >, DEVICE_GPU>;
template struct blas_gemv_batched<std::complex<double>, DEVICE_GPU>;

template struct blas_gemv_batched_strided<float , DEVICE_GPU>;
template struct blas_gemv_batched_strided<double, DEVICE_GPU>;
template struct blas_gemv_batched_strided<std::complex<float >, DEVICE_GPU>;
template struct blas_gemv_batched_strided<std::complex<double>, DEVICE_GPU>;

template struct blas_gemm<float , DEVICE_GPU>;
template struct blas_gemm<double, DEVICE_GPU>;
template struct blas_gemm<std::complex<float >, DEVICE_GPU>;
template struct blas_gemm<std::complex<double>, DEVICE_GPU>;

template struct blas_gemm_batched<float , DEVICE_GPU>;
template struct blas_gemm_batched<double, DEVICE_GPU>;
template struct blas_gemm_batched<std::complex<float >, DEVICE_GPU>;
template struct blas_gemm_batched<std::complex<double>, DEVICE_GPU>;

template struct blas_gemm_batched_strided<float , DEVICE_GPU>;
template struct blas_gemm_batched_strided<double, DEVICE_GPU>;
template struct blas_gemm_batched_strided<std::complex<float >, DEVICE_GPU>;
template struct blas_gemm_batched_strided<std::complex<double>, DEVICE_GPU>;

} // namespace op
} // namespace container