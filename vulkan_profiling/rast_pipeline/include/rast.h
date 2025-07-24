#include <string>
#include <memory>

#ifndef RAST_H
#define RAST_H

class Rasterizer {
	public:
		Rasterizer();
		~Rasterizer();
		void SetMatrices(const float* view, const float* proj, const float* model);
		void SetModelPath(const std::string& model_p);
		void SetOutputPath(const std::string& output_p);
		void run();
	private:
  	class Impl;
  	std::shared_ptr<Impl> impl_;
};

#endif // RAST_H