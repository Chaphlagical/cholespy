#include "cholesky_solver.h"
#include <nanobind/nanobind.h>
#include <nanobind/tensor.h>

#define STRINGIFY(x) #x
#define MACRO_STRINGIFY(x) STRINGIFY(x)

#define cuda_check(err) cuda_check_impl(err, __FILE__, __LINE__)

void cuda_check_impl(CUresult errval, const char *file, const int line);
namespace nb = nanobind;

// void cuda_check_impl(CUresult errval, const char *file, const int line);

template <typename Float>
void declare_cholesky(nb::module_ &m, std::string typestr) {
    using Class = CholeskySolver<Float>;
    std::string class_name = std::string("CholeskySolver") + typestr;
    nb::class_<Class>(m, class_name.c_str())
        .def("__init__", [](Class *self,
                            uint n_rows,
                            nb::tensor<int32_t, nb::shape<nb::any>, nb::device::cuda, nb::c_contig> ii,
                            nb::tensor<int32_t, nb::shape<nb::any>, nb::device::cuda, nb::c_contig> jj,
                            nb::tensor<double, nb::shape<nb::any>, nb::device::cuda, nb::c_contig> x,
                            MatrixType type){

            if (type == MatrixType::COO) {
                if (ii.shape(0) != jj.shape(0))
                    throw std::invalid_argument("Sparse COO matrix: the two index arrays should have the same size.");
                if (ii.shape(0) != x.shape(0))
                    throw std::invalid_argument("Sparse COO matrix: the index and data arrays should have the same size.");
            } else if (type == MatrixType::CSR) {
                if (jj.shape(0) != x.shape(0))
                    throw std::invalid_argument("Sparse CSR matrix: the column index and data arrays should have the same size.");
                if (ii.shape(0) != n_rows+1)
                    throw std::invalid_argument("Sparse CSR matrix: Invalid size for row pointer array.");
            } else {
                if (jj.shape(0) != x.shape(0))
                    throw std::invalid_argument("Sparse CSC matrix: the row index and data arrays should have the same size.");
                if (ii.shape(0) != n_rows+1)
                    throw std::invalid_argument("Sparse CSC matrix: Invalid size for column pointer array.");
            }

            std::vector<int> indices_a(ii.shape(0));
            std::vector<int> indices_b(jj.shape(0));
            std::vector<double> data(x.shape(0));

            cuda_check(cuMemcpyDtoHAsync(&indices_a[0], (CUdeviceptr) ii.data(), ii.shape(0)*sizeof(int), 0));
            cuda_check(cuMemcpyDtoHAsync(&indices_b[0], (CUdeviceptr) jj.data(), jj.shape(0)*sizeof(int), 0));
            cuda_check(cuMemcpyDtoHAsync(&data[0], (CUdeviceptr) x.data(), x.shape(0)*sizeof(double), 0));

            new (self) Class(n_rows, indices_a, indices_b, data, type);
        })
        .def("solve", [](Class &self,
                        nb::tensor<Float, nb::shape<nb::any, nb::any>, nb::device::cuda, nb::c_contig> b,
                        nb::tensor<Float, nb::shape<nb::any, nb::any>, nb::device::cuda, nb::c_contig> x){
            if (b.shape(0) != x.shape(0) || b.shape(1) != x.shape(1))
                throw std::invalid_argument("x and b should have the same dimensions.");
            self.solve_cuda(b.shape(1), (CUdeviceptr) b.data(), (CUdeviceptr) x.data());
        });
}

NB_MODULE(_cholesky_core, m) {

    nb::enum_<MatrixType>(m, "MatrixType")
        .value("CSC", MatrixType::CSC)
        .value("CSR", MatrixType::CSR)
        .value("COO", MatrixType::COO);

    declare_cholesky<float>(m, "F");
    declare_cholesky<double>(m, "D");

#ifdef VERSION_INFO
    m.attr("__version__") = MACRO_STRINGIFY(VERSION_INFO);
#else
    m.attr("__version__") = "dev";
#endif
}
