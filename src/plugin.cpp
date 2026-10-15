#include "plugin.hpp"  // Include the header file with plugin and model declarations


Plugin* pluginInstance;


void init(Plugin* p) {
  
    pluginInstance = p;
    
  
    p->addModel(modelFmOperator);         
    p->addModel(modelPsyOperatorFX);
    p->addModel(modelHutara_Random_CV);
    p->addModel(modelPsyKick);        


}
