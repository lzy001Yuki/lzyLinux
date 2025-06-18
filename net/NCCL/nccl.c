#include <cuda_runtime.h>
#include <nccl.h>
#include <iostream>

/**
 * @brief 使用NCCL执行多GPU数据广播操作
 * @param data 需要广播的数据缓冲区指针
 * @param count 数据元素的个数
 * @param root 广播发送方的GPU ID
 * @param comm NCCL通信器
 * @return ncclSuccess表示成功，其他错误码请参照NCCL官方文档
 */
ncclResult_t nccl_broadcast_data(void* data, size_t count, int root, ncclComm_t comm) {
    ncclErr = ncclCommUserRank(comm, &myRank);
    if (ncclErr != ncclSuccess) return ncclErr;
    cudaStream_t stream;
    cudaErr = cudaStreamCreate(&stream);
    if (cudaErr != cudaSuccess) {
        std::cerr << "CUDA Stream创建失败: " << cudaGetErrorString(cudaErr) << std::endl;
        return ncclSystemError;
    }

    // 在GPU上分配内存空间
    void *d_sendbuff = nullptr, *d_recvbuff = nullptr;
    
    // root设备发送数据
    if (myRank == root) {
        cudaErr = cudaMalloc(&d_sendbuff, count * sizeof(float));
        if (cudaErr != cudaSuccess) {
            cudaStreamDestroy(stream);
            std::cerr << "CUDA内存分配失败: " << cudaGetErrorString(cudaErr) << std::endl;
            return ncclSystemError;
        }
        
        // 主机数据拷贝
        cudaErr = cudaMemcpyAsync(d_sendbuff, data, count * sizeof(float), cudaMemcpyHostToDevice, stream);
        if (cudaErr != cudaSuccess) {
            cudaFree(d_sendbuff);
            cudaStreamDestroy(stream);
            return ncclSystemError;
        }
    }
    
    cudaErr = cudaMalloc(&d_recvbuff, count * sizeof(float));
    if (cudaErr != cudaSuccess) {
        if (myRank == root) cudaFree(d_sendbuff);
        cudaStreamDestroy(stream);
        return ncclSystemError;
    }

    // 广播
    ncclErr = ncclBroadcast(d_sendbuff, d_recvbuff, count, ncclFloat, root, comm, stream);
    
    if (ncclErr != ncclSuccess) {
        if (myRank == root) cudaFree(d_sendbuff);
        cudaFree(d_recvbuff);
        cudaStreamDestroy(stream);
        return ncclErr;
    }
    

    cudaErr = cudaStreamSynchronize(stream);
    if (cudaErr != cudaSuccess) {
        if (myRank == root) cudaFree(d_sendbuff);
        cudaFree(d_recvbuff);
        cudaStreamDestroy(stream);
        return ncclSystemError;
    }
    
    if (myRank == root) cudaFree(d_sendbuff);
    cudaFree(d_recvbuff);
    cudaStreamDestroy(stream);
    
    return ncclSuccess;
}

/**
 * @brief 使用NCCL执行多GPU数据All-Reduce操作
 * @param data 数据缓冲区指针，既作为输入也作为输出
 * @param count 数据元素的个数
 * @param comm NCCL通信器
 * @return ncclSuccess表示成功，其他错误码请参照NCCL官方文档
 */
ncclResult_t nccl_allreduce_data(void* data, size_t count, ncclComm_t comm) {
    cudaStream_t stream;
    cudaError_t cudaErr = cudaStreamCreate(&stream);
    if (cudaErr != cudaSuccess) {
        std::cerr << "CUDA Stream创建失败: " << cudaGetErrorString(cudaErr) << std::endl;
        return ncclSystemError;
    }

    void *d_sendbuff = nullptr, *d_recvbuff = nullptr;
    
    cudaErr = cudaMalloc(&d_sendbuff, count * sizeof(float));
    if (cudaErr != cudaSuccess) {
        cudaStreamDestroy(stream);
        return ncclSystemError;
    }
    
    cudaErr = cudaMalloc(&d_recvbuff, count * sizeof(float));
    if (cudaErr != cudaSuccess) {
        cudaFree(d_sendbuff);
        cudaStreamDestroy(stream);
        return ncclSystemError;
    }
    
    // 主机数据拷贝
    cudaErr = cudaMemcpyAsync(d_sendbuff, data, count * sizeof(float), cudaMemcpyHostToDevice, stream);
    if (cudaErr != cudaSuccess) {
        cudaFree(d_sendbuff);
        cudaFree(d_recvbuff);
        cudaStreamDestroy(stream);
        return ncclSystemError;
    }
    ncclResult_t ncclErr = ncclAllReduce(d_sendbuff, d_recvbuff, count, ncclFloat, ncclSum, comm, stream);
    
    if (ncclErr != ncclSuccess) {
        cudaFree(d_sendbuff);
        cudaFree(d_recvbuff);
        cudaStreamDestroy(stream);
        return ncclErr;
    }
    
    
    cudaErr = cudaStreamSynchronize(stream);
    if (cudaErr != cudaSuccess) {
        cudaFree(d_sendbuff);
        cudaFree(d_recvbuff);
        cudaStreamDestroy(stream);
        return ncclSystemError;
    }
    

    cudaFree(d_sendbuff);
    cudaFree(d_recvbuff);
    cudaStreamDestroy(stream);
    
    return ncclSuccess;
}