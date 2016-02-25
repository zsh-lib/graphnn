#ifndef LEARNER_H
#define LEARNER_H

#include "model.h"

template<MatMode mode, typename Dtype>
class ILearner
{
public:
        explicit ILearner(Model<mode, Dtype>* m, Dtype _init_lr, Dtype _l2_penalty = 0)
            : model(m), init_lr(_init_lr), l2_penalty(_l2_penalty), cur_lr(_init_lr) {}
                
        virtual void Update() = 0;                     
        
        Model<mode, Dtype>* model; 
        Dtype init_lr, l2_penalty, cur_lr;        
};

template<MatMode mode, typename Dtype>
class SGDLearner : ILearner<mode, Dtype>
{
public:
        explicit SGDLearner(Model<mode, Dtype>* m, Dtype _init_lr, Dtype _l2_penalty = 0)
                : ILearner<mode, Dtype>(m, _init_lr, _l2_penalty) {}        
        
        virtual void Update() override;                          
};


#endif