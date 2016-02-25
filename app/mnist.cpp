#include <iostream>
#include <cstdio>
#include <vector>
#include <algorithm>
#include <cstring>
#include <cmath>
#include "dense_matrix.h"
#include "nngraph.h"
#include "param_layer.h"
#include "linear_param.h"
#include "input_layer.h"
#include "relu_layer.h"
#include "classnll_criterion_layer.h"
#include "err_cnt_criterion_layer.h"
#include "model.h"
#include "learner.h"

typedef double Dtype;
const MatMode mode = GPU;
const char* f_train_feat, *f_train_label, *f_test_feat, *f_test_label;
unsigned batch_size = 100;
int dev_id;
Dtype lr = 0.001;
unsigned dim = 784;

DenseMat<mode, Dtype> input;
SparseMat<mode, Dtype> label;
NNGraph<mode, Dtype> g;
Model<mode, Dtype> model;

DenseMat<CPU, Dtype> x_cpu;
SparseMat<CPU, Dtype> y_cpu;

void LoadParams(const int argc, const char** argv)
{
	for (int i = 1; i < argc; i += 2)
	{
		if (strcmp(argv[i], "-train_feat") == 0)
			f_train_feat = argv[i + 1];
        if (strcmp(argv[i], "-train_label") == 0)
			f_train_label = argv[i + 1];
        if (strcmp(argv[i], "-test_feat") == 0)
			f_test_feat = argv[i + 1];
        if (strcmp(argv[i], "-test_label") == 0)
			f_test_label = argv[i + 1];
        if (strcmp(argv[i], "-device") == 0)
			dev_id = atoi(argv[i + 1]);                                                                
	}
}

std::vector< Dtype* > images_train, images_test;
std::vector< int > labels_train, labels_test;

void LoadRaw(const char* f_image, const char* f_label, std::vector< Dtype* >& images, std::vector< int >& labels)
{
    FILE* fid = fopen(f_image, "r");
    int buf;
    assert(fread(&buf, sizeof(int), 1, fid) == 1); // magic number
    int num;
    assert(fread(&num, sizeof(int), 1, fid) == 1); // num
    num = __builtin_bswap32(num); // the raw data is high endian    
    assert(fread(&buf, sizeof(int), 1, fid) == 1); // rows 
    assert(fread(&buf, sizeof(int), 1, fid) == 1); // cols
    images.clear();    
    unsigned char* buffer = new unsigned char[dim];
    for (int i = 0; i < num; ++i)
    {
        assert(fread(buffer, sizeof(unsigned char), dim, fid) == dim);
        Dtype* img = new Dtype[dim];
        for (unsigned j = 0; j < dim; ++j)
            img[j] = buffer[j];
        images.push_back(img);            
    }    
    delete[] buffer;
    fclose(fid);    
    
    fid = fopen(f_label, "r");
    assert(fread(&buf, sizeof(int), 1, fid) == 1); // magic number    
    assert(fread(&num, sizeof(int), 1, fid) == 1); // num
    num = __builtin_bswap32(num); // the raw data is high endian
    buffer = new unsigned char[num];
    assert(fread(buffer, sizeof(unsigned char), num, fid) == (unsigned)num);
    fclose(fid);
    labels.clear();
    for (int i = 0; i < num; ++i)
        labels.push_back(buffer[i]);    
    delete[] buffer;        
}

void InitModel()
{
    auto* h1_weight = model.add_diff< LinearParam >(dim, 1024, 0, 0.01);
    auto* h2_weight = model.add_diff< LinearParam >(1024, 1024, 0, 0.01);
    auto* o_weight = model.add_diff< LinearParam >(1024, 10, 0, 0.01);
    
    auto* input_layer = g.cl< InputLayer >("input", {});
    auto* h1 = g.cl< ParamLayer >({input_layer}, h1_weight);    
    auto* relu_1 = g.cl< ReLULayer >({h1});     
    auto* h2 = g.cl< ParamLayer >({relu_1}, h2_weight);
    auto* relu_2 = g.cl< ReLULayer >({h2});
    auto* output = g.cl< ParamLayer >({relu_2}, o_weight);
    
    g.cl< ClassNLLCriterionLayer >("classnll", {output}, true);
    g.cl< ErrCntCriterionLayer >("errcnt", {output});
}

void LoadBatch(unsigned idx_st, std::vector< Dtype* >& images, std::vector< int >& labels)
{
    x_cpu.Resize(batch_size, 784);
    y_cpu.Resize(batch_size, 10);
    y_cpu.ResizeSp(batch_size, batch_size + 1); 
    
    for (unsigned i = 0; i < batch_size; ++i)
    {
        memcpy(x_cpu.data + i * 784, images[i + idx_st], sizeof(Dtype) * 784); 
        y_cpu.data->ptr[i] = i;
        y_cpu.data->val[i] = 1.0;
        y_cpu.data->col_idx[i] = labels[i + idx_st];  
    }
    y_cpu.data->ptr[batch_size] = batch_size;
    
    input.CopyFrom(x_cpu);
    label.CopyFrom(y_cpu);
}

int main(const int argc, const char** argv)
{	
    LoadParams(argc, argv);    
	GPUHandle::Init(dev_id);
    InitModel();
    
    LoadRaw(f_train_feat, f_train_label, images_train, labels_train);
    LoadRaw(f_test_feat, f_test_label, images_test, labels_test);
    
    SGDLearner<mode, Dtype> learner(&model, lr);
            
    Dtype loss, err_rate;       
    for (int epoch = 0; epoch < 10; ++epoch)
    {
        std::cerr << "testing" << std::endl;
        loss = err_rate = 0;
        for (unsigned i = 0; i < labels_test.size(); i += batch_size)
        {
                LoadBatch(i, images_test, labels_test);
        		g.ForwardData({{"input", &input}}, TEST);                               								
				auto loss_map = g.ForwardLabel({{"classnll", &label},
                                                  {"errcnt", &label}});                
				loss += loss_map["classnll"];
                err_rate += loss_map["errcnt"];
        }
        loss /= labels_test.size();
        err_rate /= labels_test.size();
        std::cerr << fmt::sprintf("test loss: %.4f\t error rate: %.4f", loss, err_rate) << std::endl;
        
        for (unsigned i = 0; i < labels_train.size(); i += batch_size)
        {
                LoadBatch(i, images_train, labels_train);
                g.ForwardData({{"input", &input}}, TRAIN);
                auto loss_map = g.ForwardLabel({{"classnll", &label}});
				loss = loss_map["classnll"] / batch_size;
                
                g.BackPropagation();
                learner.Update();                                             
        }
    }
    
    GPUHandle::Destroy();
	return 0;    
}