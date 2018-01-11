#include "myMosqConcrete.h"

//Add flag that while processing, should not accept more message or do not process incoming
myMosqConcrete::myMosqConcrete(const char* id, const char* _topic, const char* host, int port, Utils::Configs c)
        : myMosq(id, _topic, host, port) {
    this->c = c;
    std::cout << "Setup of mosquitto." << std::endl;
}

// struct mosquitto_message{
//         int mid;
//         char *topic;
//         void *payload;
//         int payloadlen;
//         int qos;
//         bool retain;
// }
bool myMosqConcrete::receive_message(const struct mosquitto_message* message) {
    std::cout << "Slave node start processing!" << std::endl;
    char *pchar = (char*)(message->payload);
    std::string receivedTopic(message->topic);
    std::string nodeTopic = c.nodeName;
    std::cout << receivedTopic << std::endl;
    if (receivedTopic.find("slave/" + this->c.nodeName) != std::string::npos) {
        std::cout << "Received data from master sent to: " + this->c.nodeName << std::endl;
        initiateTraining(pchar);
    } else if (receivedTopic.find("flask/mqtt/query") != std::string::npos) {
        std::stringstream ss;
        ss << "Response, i'm here" << std::endl;
        const std::string& tmp = ss.str();
        const char* cstr = tmp.c_str();
        std::cout << "Received flask: " + tmp;
        this->send_message(nodeTopic.c_str(), cstr);
    }
    //End of MQTT

    return true;
}

void myMosqConcrete::initiateTraining(const char* pchar) {
    writeToFile(pchar, "data.txt");
    // サンプルデータの読み込み
    //
    Utils::Timer* t = new Utils::Timer();
    t->start();
    train();
    t->stop();
    delete t;

    std::cout << "Slave node done training, written to data.txt" << std::endl;

    //Train first before sending
    //Read dataN.txt files to buffer and publish via MQTT.
    //send to master topic in localhost
    char dir[255];
    getcwd(dir,255);
    std::stringstream ss;
    ss << dir << "/RTs_Forest.txt";
    char* buffer = fileToBuffer(ss.str());

    std::string topic("master/");
    topic += c.nodeName;

    //remove "slave/" from part of name (what a hack)
    int indexToRemove = topic.find("slave/");
    topic.erase(indexToRemove, 6);

    std::cout << "Publishing to topic: " << topic << std::endl;
    this->send_message(topic.c_str(), buffer);
    delete[] buffer;
}


//TODO: make arguments adjustable via argv and transfer code to pi to start distribution
int myMosqConcrete::train() {
    std::cout << "start training" << std::endl;
    Utils::Parser *p = new Utils::Parser();
    //can also put the class column info into config
    p->setClassColumn(1);
    std::vector<RTs::Sample> samples;

    char dir[255];
    getcwd(dir,255);
    std::stringstream ss;
    ss << dir << "/data.txt";
    std::cout << ss.str() << std::endl;
    samples = p->readCSVToSamples(ss.str());
    std::cout << samples[4].label << std::endl;

    std::cout << "1_Randomized Forest generation" << std::endl;
    RTs::Forest rts_forest;
    if(!rts_forest.Learn(
                    this->c.numClass,
                    this->c.numTrees,
                    this->c.maxDepth,
                    this->c.featureTrials,
                    this->c.thresholdTrials,
                    this->c.dataPerTree, 
                    samples)){
        printf("Randomized Forest Failed generation\n");
        std::cerr << "RTs::Forest::Learn() failed." << std::endl;
        std::cerr.flush();
        return 1;
    }

    std::cout << "2_Saving the learning result" << std::endl;
    if(rts_forest.Save("RTs_Forest.txt") == false){
        std::cerr << "RTs::Forest::Save() failed." << std::endl;
        std::cerr.flush();
        return 1;
    }

    return 0;
}
