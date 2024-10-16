#include <stdio.h>
#include <string>
#include <map>
#include <set>

#include <uuid/uuid.h>
#include "utils/uuid.h"
#include "mapcore/hdmap.h"
#include "mapcore/sdmap.h"

#include "com_civip_hdmap_jni_JNIHDMapRender.h"

using namespace std;
using namespace zas::mapcore;

JNIEXPORT jlong JNICALL Java_com_civip_hdmap_jni_JNIHDMapRender_createNativeMapRender
  (JNIEnv *env, jobject jobj, jstring jpath)
{
	jlong result;
	const char* path = env->GetStringUTFChars(jpath, nullptr);
	rendermap* rmap = new rendermap();
	result = (jlong)rmap;
	int ret = rmap->load_fromfile(path);
	if (ret == 0) {
		return result;
	}
	return 0;
}

JNIEXPORT jintArray JNICALL Java_com_civip_hdmap_jni_JNIHDMapRender_getBlockDepends
  (JNIEnv *env, jobject jobj, jlong maprenderaddr, jintArray jblockids)
{
	jboolean isCopy = false;
	set<uint32_t> reqblockids;
	set<uint32_t> depblockids;
	jsize size = env->GetArrayLength(jblockids);
	jint *idp = env->GetIntArrayElements(jblockids, &isCopy);
	for (jint i = 0; i < size; i++) {
		reqblockids.insert(idp[i]);
	}

	auto rmap = reinterpret_cast<rendermap*>(maprenderaddr);
	if (nullptr == rmap) {
		jintArray retarr = env->NewIntArray(0);
		return retarr;
	} 
	else {
		rmap->get_block_dependencies(reqblockids, depblockids);
		jsize len = depblockids.size();
		jintArray retarr = env->NewIntArray(len);

		jint idarray[len];
		jint index = 0;
		for (set<uint32_t>::iterator it = depblockids.begin(); 
			it != depblockids.end(); ++it) {
			idarray[index++] = *it; 
		}

		jint *idset = idarray;
		env->SetIntArrayRegion(retarr, 0, len, idset);
		return retarr;
	}
}

JNIEXPORT jbyteArray JNICALL Java_com_civip_hdmap_jni_JNIHDMapRender_getHDMapInfo
  (JNIEnv *env, jobject jobj, jlong maprenderaddr, jintArray reqblockids, jintArray depblockids)
{
	jboolean isCopy = false;
	set<uint32_t> reqblockidset;
	set<uint32_t> depblockidset;
	reqblockidset.clear();
	if (nullptr != reqblockids) {
		jsize reqsize = env->GetArrayLength(reqblockids);
		jint *idpreq = env->GetIntArrayElements(reqblockids, &isCopy);
		for (jint i = 0; i < reqsize; i++) {
			reqblockidset.insert(idpreq[i]);
		}
	}
	depblockidset.clear();
	if (nullptr != depblockids) {
		jsize depsize = env->GetArrayLength(depblockids);
		jint *iddep = env->GetIntArrayElements(depblockids, &isCopy);
		for (jint i = 0; i < depsize; i++) {
			depblockidset.insert(iddep[i]);
		}
	}
	std::string buf;
	buf.clear();
	auto rmap = reinterpret_cast<rendermap*>(maprenderaddr);
	if (nullptr == rmap) {
		jbyteArray retarr = env->NewByteArray(0);
		return retarr;
	} 
	else {
		rmap->extract_blocks(reqblockidset, depblockidset, buf);
		jsize size = buf.length();

		const char* array = buf.c_str();
		size_t array_sz = buf.length();
		const uint8_t* buf8 = reinterpret_cast<const uint8_t*>(buf.c_str());
		jbyteArray retarr = env->NewByteArray(size);
		env->SetByteArrayRegion(retarr, 0 , size, (jbyte*)buf8);
		return retarr;
	}
}

JNIEXPORT jobject JNICALL Java_com_civip_hdmap_jni_JNIHDMapRender_getMapHeadInfo
  (JNIEnv *env, jobject jobj, jlong maprenderaddr)
{
	jclass mapHeadClass = env->FindClass("com/civip/hdmap/jni/MapHead");
    jmethodID init = env->GetMethodID(mapHeadClass, "<init>", "()V");
    jobject maphead = env->NewObject(mapHeadClass, init);
	auto rmap = reinterpret_cast<rendermap*>(maprenderaddr);
	if (nullptr != rmap) {
		rendermap_info minfo;
		int ret = rmap->get_mapinfo(minfo);
		if(ret == 0) {
			jfieldID uuid = env->GetFieldID(mapHeadClass, "uuid", "Ljava/lang/String;");
    		jfieldID col = env->GetFieldID(mapHeadClass, "col", "I");
			jfieldID row = env->GetFieldID(mapHeadClass, "row", "I");
			jfieldID junctionCount = env->GetFieldID(mapHeadClass, "junctionCount", "J");
			jfieldID xmin = env->GetFieldID(mapHeadClass, "xmin", "D");
			jfieldID ymin = env->GetFieldID(mapHeadClass, "ymin", "D");
			jfieldID xmax = env->GetFieldID(mapHeadClass, "xmax", "D");
			jfieldID ymax = env->GetFieldID(mapHeadClass, "ymax", "D");

			zas::utils::uuid tmpuid;
			tmpuid = minfo.uuid;
			std::string mapuuid;
			tmpuid.to_hex(mapuuid);

			env->SetObjectField(maphead, uuid, env->NewStringUTF(mapuuid.c_str()));
			env->SetIntField(maphead, col, minfo.cols);
			env->SetIntField(maphead, row, minfo.rows);
			env->SetLongField(maphead, junctionCount, minfo.junction_count);
			env->SetDoubleField(maphead, xmin, minfo.xmin);
			env->SetDoubleField(maphead, ymin, minfo.ymin);
			env->SetDoubleField(maphead, xmax, minfo.xmax);
			env->SetDoubleField(maphead, ymax, minfo.ymax);
		}
	}
	return maphead;
}

JNIEXPORT jobjectArray JNICALL Java_com_civip_hdmap_jni_JNIHDMapRender_getMapSingleLight
  (JNIEnv *env, jobject jobj, jlong maprenderaddr)
{
	jobjectArray lightArr;
	auto rmap = reinterpret_cast<rendermap*>(maprenderaddr);
	if (nullptr != rmap) {
		rendermap_info info;
		int ret = rmap->get_mapinfo(info);
		if(ret == 0) {
			uint32_t cols = info.cols;
			uint32_t rows = info.rows;
			set<const rendermap_roadobject*> roret;
			for(uint32_t index = 0; index < cols * rows; index++) {
				ret = rmap->extract_roadobjects(index, {hdrm_robjtype_signal_light}, roret, false);
				if(ret == 0) {
					jclass lightClass = env->FindClass("com/civip/hdmap/jni/SingleLight");
					jmethodID init = env->GetMethodID(lightClass, "<init>", "()V");
					jobject singleLight = env->NewObject(lightClass, init);
					lightArr = env->NewObjectArray(roret.size(), lightClass, singleLight);
					uint32_t index = 0;
					for (auto ro : roret) {
						singleLight = env->NewObject(lightClass, init);
						
						const hdrmap_ro_signal_light *light = ro->as_siglight();
						point3d p3d;
						light->getpos(p3d);
						double mtx;
						light->get_rotate_matrix(&mtx);

						jfieldID id = env->GetFieldID(lightClass, "id", "Ljava/lang/String;");
						jfieldID x = env->GetFieldID(lightClass, "x", "D");
						jfieldID y = env->GetFieldID(lightClass, "y", "D");
						jfieldID z = env->GetFieldID(lightClass, "z", "D");
						jfieldID rotate = env->GetFieldID(lightClass, "rotate", "D");

						env->SetObjectField(singleLight, id, env->NewStringUTF(to_string(light->getuid()).c_str()));
						env->SetDoubleField(singleLight, x, p3d.xyz.x);
						env->SetDoubleField(singleLight, y, p3d.xyz.y);
						env->SetDoubleField(singleLight, z, p3d.xyz.z);
						env->SetDoubleField(singleLight, rotate, mtx);

						env->SetObjectArrayElement(lightArr, index, singleLight);
						index++;
					}
				}
			}
		}
	}
	return lightArr;
}

JNIEXPORT jobjectArray JNICALL Java_com_civip_hdmap_jni_JNIHDMapRender_getHDMapJunction
  (JNIEnv *env, jobject jobj, jlong maprenderaddr, jstring fullmap)
{
	jobjectArray juncArr;
	auto rmap = reinterpret_cast<rendermap*>(maprenderaddr);
	if (nullptr != rmap) {
		sdmap smap;
		rendermap_info minfo;
		int ret = rmap->get_mapinfo(minfo);
		double xoff,yoff;
		rmap->get_offset(xoff, yoff);
		if(ret == 0) {
			jboolean bo = false;
			smap.load_fromfile(env->GetStringUTFChars(fullmap, &bo));
			jclass junctionClass = env->FindClass("com/civip/hdmap/jni/Junction");
    		jmethodID init = env->GetMethodID(junctionClass, "<init>", "()V");
			jobject junction = env->NewObject(junctionClass, init);
			juncArr = env->NewObjectArray(minfo.junction_count, junctionClass, junction);
			for (int i = 0; i < minfo.junction_count; ++i) {
				junction = env->NewObject(junctionClass, init);

				auto junc = rmap->get_junction_by_index(i);
				assert(nullptr != junc);
				double jx, jy, jr;
				junc->center_point(jx, jy, jr);

				vector<const char*> names;
				ret = junc->extract_desc(&smap, rmap, names);
				std::string namestr;
				for(int j = 0;j < names.size(); j++) {
					namestr += names[j];
				}
				jstring n = env->NewStringUTF(namestr.c_str());

				jfieldID id = env->GetFieldID(junctionClass, "id", "J");
				jfieldID x = env->GetFieldID(junctionClass, "x", "D");
				jfieldID y = env->GetFieldID(junctionClass, "y", "D");
				jfieldID r = env->GetFieldID(junctionClass, "r", "D");
				jfieldID name = env->GetFieldID(junctionClass, "names" ,"Ljava/lang/String;");

				env->SetLongField(junction, id, junc->getid());
				env->SetDoubleField(junction, x, (jx + xoff));
				env->SetDoubleField(junction, y, (jy + yoff));
				env->SetDoubleField(junction, r, jr);
				env->SetObjectField(junction, name, n);

				env->SetObjectArrayElement(juncArr, i, junction);
			}
		}
	}
	return juncArr;
}

JNIEXPORT jobject JNICALL Java_com_civip_hdmap_jni_JNIHDMapRender_getJunctionByPos
  (JNIEnv *env, jobject jobj, jlong maprenderaddr, jstring fullmap, jdouble x, jdouble y, jint count)
{
	jclass junctionClass = env->FindClass("com/civip/hdmap/jni/Junction");
    jmethodID init = env->GetMethodID(junctionClass, "<init>", "()V");
    jobject junction = env->NewObject(junctionClass, init);
	auto rmap = reinterpret_cast<rendermap*>(maprenderaddr);
	if (nullptr != rmap) {
		sdmap smap;
		vector<rendermap_nearest_junctions> junc;
		double xoff,yoff;
		rmap->get_offset(xoff, yoff);
		int ret = rmap->find_junctions((x - xoff), (y - yoff), count, junc);
		if (ret > 0) {
			jboolean bo = false;
			ret = smap.load_fromfile(env->GetStringUTFChars(fullmap, &bo));
			rendermap_nearest_junctions jun = junc[0];
			double jx, jy, jr;
			jun.junction->center_point(jx, jy, jr);
			vector<const char*> names;
			jun.junction->extract_desc(&smap, rmap, names);
			// jcharArray namearr = env->NewCharArray(names.size());
			std::string namestr;
			for(int i = 0;i < names.size(); i++) {
				// env->SetCharArrayRegion(namearr, i, 1, (jchar*)names[i]);
				namestr += names[i];
				namestr += " ";
			}
			jstring n = env->NewStringUTF(namestr.c_str());

			jfieldID id = env->GetFieldID(junctionClass, "id", "J");
			jfieldID x = env->GetFieldID(junctionClass, "x", "D");
			jfieldID y = env->GetFieldID(junctionClass, "y", "D");
			jfieldID r = env->GetFieldID(junctionClass, "r", "D");
			jfieldID name = env->GetFieldID(junctionClass, "names" ,"Ljava/lang/String;");

			env->SetLongField(junction, id, jun.junction->getid());
			env->SetDoubleField(junction, x, (jx + xoff));
			env->SetDoubleField(junction, y, (jy + yoff));
			env->SetDoubleField(junction, r, jr);
			env->SetObjectField(junction, name, n);
		}
	}
	return junction;
}