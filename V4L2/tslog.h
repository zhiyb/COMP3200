#ifndef TSLOG_H
#define TSLOG_H

#include <vector>
#include <iostream>

#define TS_AVERAGE	2

class TSLog
{
public:
	void add(float time)
	{
		using namespace std;
		float fps = d.empty() ? 0.f : 1.f / (time - last());
		if (d.size() >= TS_AVERAGE * 2) {
			//clog << d.size() << ", " << fps << endl;
			float fsum = 0.f;
			for (int i = 0; i != TS_AVERAGE; i++)
				fsum += fmin(d[d.size() - i * 2 - 2], 30.f);
			fsum += fps;
			fps = fsum / (float)(TS_AVERAGE + 1);
		}
		d.push_back(fps);
		//std::clog << last() << ", ";
		d.push_back(time);
		//std::clog << last() << std::endl;
		while (d.size() > 900)
			d.erase(d.begin());
	}

	unsigned long size() {return d.size() * sizeof(float);}
	unsigned long count() {return d.size() / 2;}
	float *data() {return d.data();}
	float last() {return d.back();}

private:
	std::vector<float> d;
};

#endif
