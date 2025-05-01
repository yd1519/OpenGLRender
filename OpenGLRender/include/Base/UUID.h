#ifndef UUID_H
#define UUID_H

namespace OpenGL {

//为不同类型生成UUID
template<class T>
class UUID {
public:
	UUID() : uuid_(uuidCounter_++){}

	inline int get() const {
		return uuid_;
	}


private:
	int uuid_ = -1;
	static int uuidCounter_;
};

template<class T>
int UUID<T>::uuidCounter_ = 0;


}

#endif