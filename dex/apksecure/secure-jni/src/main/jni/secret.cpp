#include <jni.h>
#include <string>
#include <android/log.h>
#include "logutils.h"

const jbyte pwd[100]="laChineestunlionendormi";
const char* encryptedFileName = "apksecurefile";



extern "C" {
void replaceDefaultClassLoader(JNIEnv *env, jobject instance, jstring dexFilePath,jint build_version);

jobject NewFile(JNIEnv *env,jclass FileClass,jstring path) {
    jmethodID FileMethodId = env->GetMethodID(FileClass, "<init>", "(Ljava/lang/String;)V");
    return env->NewObject(FileClass, FileMethodId, path);
}

/**
 * 解密算法
 * sourceFilePath:加密的文件路径
 * destFilePath:解密后的文件路径
 */
void decrypt(JNIEnv* env,jstring sourceFilePath,jstring destFilePath){
    jclass CipherClass = env->FindClass("javax/crypto/Cipher");
    jmethodID getInstanceId = env->GetStaticMethodID(CipherClass,"getInstance","(Ljava/lang/String;)Ljavax/crypto/Cipher;");
    jobject cipher = env->CallStaticObjectMethod(CipherClass,getInstanceId,env->NewStringUTF("DES"));

    jclass DESKeySpecClass = env->FindClass("javax/crypto/spec/DESKeySpec");
    jmethodID initDESKeySpecId = env->GetMethodID(DESKeySpecClass,"<init>","([B)V");

    jbyteArray jPwd=env->NewByteArray(100);
    env->SetByteArrayRegion(jPwd,0,100,pwd);
    jobject desKeySpec = env->NewObject(DESKeySpecClass,initDESKeySpecId,jPwd);

    jclass SecretKeyFactoryClass = env->FindClass("javax/crypto/SecretKeyFactory");
    jmethodID getSKFactoryInstanceId = env->GetStaticMethodID(SecretKeyFactoryClass,"getInstance","(Ljava/lang/String;)Ljavax/crypto/SecretKeyFactory;");
    jobject secretKeyFactory = env->CallStaticObjectMethod(SecretKeyFactoryClass,getSKFactoryInstanceId,env->NewStringUTF("DES"));
    jmethodID generateSecretId = env->GetMethodID(SecretKeyFactoryClass,"generateSecret","(Ljava/security/spec/KeySpec;)Ljavax/crypto/SecretKey;");
    jobject key = env->CallObjectMethod(secretKeyFactory,generateSecretId,desKeySpec);

    jmethodID initId = env->GetMethodID(CipherClass,"init","(ILjava/security/Key;)V");
    env->CallVoidMethod(cipher,initId,2,key);

    jclass FileInputStreamClass = env->FindClass("java/io/FileInputStream");
    jmethodID initFileInputStream = env->GetMethodID(FileInputStreamClass,"<init>","(Ljava/lang/String;)V");
    jobject fis = env->NewObject(FileInputStreamClass,initFileInputStream,sourceFilePath);

    jclass FileOutputStreamClass = env->FindClass("java/io/FileOutputStream");
    jmethodID initFileOutputStream = env->GetMethodID(FileOutputStreamClass,"<init>","(Ljava/lang/String;)V");
    jobject fos = env->NewObject(FileOutputStreamClass,initFileOutputStream,destFilePath);

    jclass CipherOutputStreamClass = env->FindClass("javax/crypto/CipherOutputStream");
    jmethodID initCipherOutputStream = env->GetMethodID(CipherOutputStreamClass,"<init>","(Ljava/io/OutputStream;Ljavax/crypto/Cipher;)V");
    jobject cos= env->NewObject(CipherOutputStreamClass,initCipherOutputStream,fos,cipher);

    jmethodID fisRead = env->GetMethodID(env->GetObjectClass(fis),"read","([B)I");
    jmethodID cosWrite = env->GetMethodID(CipherOutputStreamClass,"write","([BII)V");
    jmethodID cosFlush = env->GetMethodID(CipherOutputStreamClass,"flush","()V");
    jmethodID fosClose = env->GetMethodID(FileOutputStreamClass,"close","()V");
    jmethodID cosClose = env->GetMethodID(CipherOutputStreamClass,"close","()V");
    jmethodID fisClose = env->GetMethodID(env->GetObjectClass(fis),"close","()V");

    jbyteArray buffer = env->NewByteArray(1024);
    jint bufferSize = 0;
    while(bufferSize!=-1){
        bufferSize = env->CallIntMethod(fis,fisRead,buffer);
        LOGE("读取 %d 个字节",bufferSize);
        if(bufferSize>0){
            env->CallVoidMethod(cos,cosWrite,buffer,0,bufferSize);
            LOGE("解密 %d 个字节",bufferSize);
        }
    }

    env->CallVoidMethod(cos,cosFlush);
    env->CallVoidMethod(cos,cosClose);
    if(env->ExceptionCheck()){
        env->ExceptionDescribe();
        env->ExceptionClear();
        env->ThrowNew(env->FindClass("java/lang/Exception"),"解密失败");
    }
    env->CallVoidMethod(fos,fosClose);
    env->CallVoidMethod(fis,fisClose);
}

void unzipFile(JNIEnv *env, jstring zipPath, jstring folderPath) {
    jclass ZipUtilsClass = env->FindClass("dev/mars/secure/ZipUtils");
    if(ZipUtilsClass==NULL){
        LOGE("未找到ZipUtilsClass");
        return;
    }
    jmethodID unzipFileId = env->GetStaticMethodID(ZipUtilsClass,"unzipFile","(Ljava/lang/String;Ljava/lang/String;)Ljava/util/List;");
    if(unzipFileId==NULL){
        LOGE("未找到unzipFileId");
        return ;
    }
    env->CallStaticObjectMethod(ZipUtilsClass,unzipFileId,zipPath,folderPath);
}

jboolean isFileExist(JNIEnv *env, jobject file) {
    jclass FileClass = env->GetObjectClass(file);
    jmethodID existsMethodId = env->GetMethodID(FileClass, "exists", "()Z");
    return env->CallBooleanMethod(file, existsMethodId);
}

jstring getOriginDexPath(JNIEnv *env, jobject instance){
    //得到Application的class
    jclass appClass = env->GetObjectClass(instance);
    //得到getDir方法
    jmethodID getDirMethodId = env->GetMethodID(appClass,"getDir","(Ljava/lang/String;I)Ljava/io/File;");
    if(getDirMethodId==NULL){
        LOGE("未找到getDir");
        return NULL;
    }
    //得到dex的文件夹路径
    jstring dexFolderNameStr = env->NewStringUTF("mdex");
    jint mode = 0;
    jobject dexFolderFile = env->CallObjectMethod(instance, getDirMethodId, dexFolderNameStr, mode);
    jclass FileClass = env->GetObjectClass(dexFolderFile);
    jmethodID getAbsolutePathMethodId = env->GetMethodID(FileClass,"getAbsolutePath","()Ljava/lang/String;");
    if(getAbsolutePathMethodId==NULL){
        LOGE("未找到getAbsolutePathMethodId");
        return NULL;
    }

    jstring dexFolderPathJStr = (jstring) env->CallObjectMethod(dexFolderFile, getAbsolutePathMethodId);
    const char* dexFolderPath = env->GetStringUTFChars(dexFolderPathJStr,false);

    jmethodID listFileNames = env->GetMethodID(FileClass,"list","()[Ljava/lang/String;");
    jmethodID endsWith = env->GetMethodID(env->GetObjectClass(dexFolderPathJStr),"endsWith","(Ljava/lang/String;)Z");

    jstring dexFormat = env->NewStringUTF(".dex");
    jobjectArray dexFileNames = (jobjectArray) env->CallObjectMethod(dexFolderFile, listFileNames);
    jint totalFiles = env->GetArrayLength(dexFileNames);
    std::string dexPaths="";
    if(dexFileNames!=NULL&&totalFiles>0){
        for(int i=0;i <totalFiles;i++){
            jstring fileNameJStr = (jstring) env->GetObjectArrayElement(dexFileNames, i);
            const char * fileName = env->GetStringUTFChars(fileNameJStr, false);
            LOGE("检测到文件 : %s",fileName);
            if(env->CallBooleanMethod(fileNameJStr,endsWith,dexFormat)){
                dexPaths.append(dexFolderPath);
                dexPaths.append("/");
                dexPaths.append(fileName);
                dexPaths.append(":");
            }
        }
        LOGE("DEX路径 : %s",dexPaths.c_str());
        if(dexPaths.length()>0) {
            return env->NewStringUTF(dexPaths.c_str());
        }
    }
    LOGE("未找到原DEX，需要解压");

    //从assets文件夹中找到abc.zip，并复制到dexFolderPath
    jmethodID getAssetsId = env->GetMethodID(appClass,"getAssets","()Landroid/content/res/AssetManager;");
    if(getAssetsId==NULL){
        LOGE("未找到getAssetsId");
        return NULL;
    }
    jobject assetsManager = env->CallObjectMethod(instance,getAssetsId);
    jmethodID openId = env->GetMethodID(env->GetObjectClass(assetsManager),"open","(Ljava/lang/String;)Ljava/io/InputStream;");
    if(openId==NULL){
        LOGE("未找到openId");
        return NULL;
    }
    jstring encryptedFileNameJStr = env->NewStringUTF(encryptedFileName);
    jobject is = env->CallObjectMethod(assetsManager,openId,encryptedFileNameJStr);
    if(env->ExceptionCheck()){
        env->ExceptionDescribe();
        env->ExceptionClear();
        env->ThrowNew(env->FindClass("java/lang/Exception"),"未找到文件");
        return NULL;
    }
    jclass FileOutputStreamClass = env->FindClass("java/io/FileOutputStream");
    if(FileOutputStreamClass==NULL){
        LOGE("未找到FileOutputSteamClass");
        return NULL;
    }
    jmethodID initFileOutputStreamClass = env->GetMethodID(FileOutputStreamClass,"<init>","(Ljava/lang/String;)V");
    std::string encryptedFileCopyPath = env->GetStringUTFChars(dexFolderPathJStr, false);
    encryptedFileCopyPath.append(encryptedFileName);
    LOGE("加密文件复制后的路径:%s",encryptedFileCopyPath.c_str());
    jstring encryptedFileCopyPathJStr =env->NewStringUTF(encryptedFileCopyPath.c_str());
    jobject fos = env->NewObject(FileOutputStreamClass,initFileOutputStreamClass,encryptedFileCopyPathJStr);
    if(fos==NULL){
        LOGE("创建fos失败");
    }
    if(is!=NULL&&fos!=NULL){
        LOGE("创建is fos成功");
    }
    jmethodID readId = env->GetMethodID(env->GetObjectClass(is),"read","([B)I");
    jmethodID writeId = env->GetMethodID(FileOutputStreamClass,"write","([BII)V");
    jmethodID flushId = env->GetMethodID(FileOutputStreamClass,"flush","()V");
    jmethodID fosCloseId = env->GetMethodID(FileOutputStreamClass,"close","()V");
    jmethodID isCloseId = env->GetMethodID(env->GetObjectClass(is),"close","()V");

    jbyteArray buffer = env->NewByteArray(1024);
    jint bufferSize = 0;
    while(bufferSize!=-1){
        bufferSize = env->CallIntMethod(is,readId,buffer);
        //LOGE("读取 %d 个字节",bufferSize);
        if(bufferSize>0){
            env->CallVoidMethod(fos,writeId,buffer,0,bufferSize);
            //LOGE("写出 %d 个字节",bufferSize);
        }
    }
    env->CallVoidMethod(fos,flushId);
    env->CallVoidMethod(is,isCloseId);
    env->CallVoidMethod(fos,fosCloseId);

    jobject encryptedFile = NewFile(env,FileClass,encryptedFileCopyPathJStr);
    if(isFileExist(env,encryptedFile)){
        LOGE("成功复制文件:%s",encryptedFileCopyPath.c_str());
    }else{
        LOGE("未找到文件:%s",encryptedFileCopyPath.c_str());
    }

    std::string zipFilePath = env->GetStringUTFChars(dexFolderPathJStr, false);
    zipFilePath.append("/abc.zip");
    jstring zipFilePathJStr = env->NewStringUTF(zipFilePath.c_str());

    decrypt(env,encryptedFileCopyPathJStr,zipFilePathJStr);

    jobject abcFile = NewFile(env,FileClass,zipFilePathJStr);
    if(isFileExist(env,abcFile)){
        LOGE("成功解密文件:%s",zipFilePath.c_str());
    }else{
        LOGE("未找到文件:%s",zipFilePath.c_str());
    }

    unzipFile(env,zipFilePathJStr,dexFolderPathJStr);

    jmethodID fileDelete = env->GetMethodID(FileClass,"delete","()Z");
    jboolean r1 = env->CallBooleanMethod(abcFile, fileDelete);
    jboolean r2 = env->CallBooleanMethod(encryptedFile,fileDelete);
    if(r1&&r2) {
        LOGE("删除加密文件和压缩文件");
    }
    dexPaths.clear();
    dexFileNames = (jobjectArray) env->CallObjectMethod(dexFolderFile, listFileNames);
    totalFiles = env->GetArrayLength(dexFileNames);
    if(dexFileNames!=NULL&&totalFiles>0){
        for(int i=0;i <totalFiles;i++){
            jstring fileNameJStr = (jstring) env->GetObjectArrayElement(dexFileNames, i);
            const char * fileName = env->GetStringUTFChars(fileNameJStr, false);
            LOGE("检测到文件 : %s",fileName);
            if(env->CallBooleanMethod(fileNameJStr,endsWith,dexFormat)){
                dexPaths.append(dexFolderPath);
                dexPaths.append("/");
                dexPaths.append(fileName);
                dexPaths.append(":");
            }
        }
        LOGE("DEX路径 : %s",dexPaths.c_str());
        if(dexPaths.length()>0) {
            return env->NewStringUTF(dexPaths.c_str());
        }
    }
    return NULL;
}

JNIEXPORT void JNICALL
Java_dev_mars_secure_ProxyApplication_onAttachBaseContext(JNIEnv *env, jobject instance,jint build_version) {
    //找到原classes.dex
    jstring dexFilePaths =getOriginDexPath(env,instance);
    if(dexFilePaths==NULL&&env->GetStringLength(dexFilePaths)<=0){
        LOGE("未成功生成DEX");
        return;
    }
    //到这一步为止，classes.dex已经成功从zip中解密并解压到 dexFilePath,下一步开始替换默认的PathClassLoader
    replaceDefaultClassLoader(env, instance,dexFilePaths,build_version);
}


void replaceDefaultClassLoader(JNIEnv *env, jobject instance, jstring dexFilePath,jint build_version) {
    //得到ActivityThread实例
    jclass ActivityThreadClass = env->FindClass("android/app/ActivityThread");
    jmethodID currentActivityThread=env->GetStaticMethodID(ActivityThreadClass,"currentActivityThread","()Landroid/app/ActivityThread;");
    jobject activityThread = env->CallStaticObjectMethod(ActivityThreadClass,currentActivityThread);

    jmethodID getPackageName = env->GetMethodID(env->GetObjectClass(instance),"getPackageName","()Ljava/lang/String;");
    jstring packageName = (jstring) env->CallObjectMethod(instance, getPackageName);

    jfieldID mPackagesField;
    if(build_version<19){
        mPackagesField = env->GetFieldID(ActivityThreadClass,"mPackages","Ljava/util/HashMap;");
    }else{
        mPackagesField = env->GetFieldID(ActivityThreadClass,"mPackages","Landroid/util/ArrayMap;");
    }

    jobject mPackages = env->GetObjectField(activityThread,mPackagesField);

    jmethodID arrayMapGet = env->GetMethodID(env->GetObjectClass(mPackages),"get","(Ljava/lang/Object;)Ljava/lang/Object;");
    jobject wr = env->CallObjectMethod(mPackages,arrayMapGet,packageName);

    jmethodID getDir = env->GetMethodID(env->GetObjectClass(instance),"getDir","(Ljava/lang/String;I)Ljava/io/File;");
    jstring optimizedDirName = env->NewStringUTF("optimizedDirectory");
    jobject optimizedDirFolder = env->CallObjectMethod(instance,getDir,optimizedDirName,0);
    jmethodID getAbsolutePath = env->GetMethodID(env->GetObjectClass(optimizedDirFolder),"getAbsolutePath","()Ljava/lang/String;");
    jstring optimizedDirFolderPath = (jstring) env->CallObjectMethod(optimizedDirFolder, getAbsolutePath);

    jmethodID getApplicationInfo = env->GetMethodID(env->GetObjectClass(instance),"getApplicationInfo","()Landroid/content/pm/ApplicationInfo;");
    jobject appInfo = env->CallObjectMethod(instance,getApplicationInfo);
    jfieldID nativeLibraryDirField= env->GetFieldID(env->GetObjectClass(appInfo),"nativeLibraryDir","Ljava/lang/String;");
    jstring nativeLibraryDir = (jstring) env->GetObjectField(appInfo, nativeLibraryDirField);

    jmethodID weakReferenceGet = env->GetMethodID(env->GetObjectClass(wr),"get","()Ljava/lang/Object;");
    jobject loadedApk = env->CallObjectMethod(wr,weakReferenceGet);
    jfieldID mClassLoaderField = env->GetFieldID(env->GetObjectClass(loadedApk),"mClassLoader","Ljava/lang/ClassLoader;");
    jobject mClassLoader = env->GetObjectField(loadedApk,mClassLoaderField);

    //分割dexFilePath
    jmethodID split=env->GetMethodID(env->GetObjectClass(dexFilePath),"split","(Ljava/lang/String;)[Ljava/lang/String;");
    //jobjectArray paths = (jobjectArray) env->CallObjectMethod(dexFilePath, split, env->NewStringUTF(":"));

    jclass DexClassLoaderClass = env->FindClass("dalvik/system/DexClassLoader");
    jmethodID initDexClassLoader  = env->GetMethodID(DexClassLoaderClass,"<init>","(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/ClassLoader;)V");
    LOGE("开始加载DEX %s",env->GetStringUTFChars(dexFilePath,false));

    //默认用以:分割的路径加载，如果加载的dex很大，在Android5.0以上很耗时
    jobject dexClassLoader= env->NewObject(DexClassLoaderClass,initDexClassLoader,dexFilePath,optimizedDirFolderPath,nativeLibraryDir,mClassLoader);

    //将DEX依次加载
    /*
    jobject dexClassLoader = mClassLoader;
    int totalPath = env->GetArrayLength(paths);
    if(paths!=NULL&&totalPath>0){
        for(int i=0;i <totalPath;i++){
            jstring fileNameJStr = (jstring) env->GetObjectArrayElement(paths, i);
            const char * fileName = env->GetStringUTFChars(fileNameJStr, false);
            LOGE("加载第%d个DEX : %s",i,fileName);
            dexClassLoader = env->NewObject(DexClassLoaderClass,initDexClassLoader,fileNameJStr,optimizedDirFolderPath,nativeLibraryDir,dexClassLoader);
        }
    }
     */
    LOGE("所有DEX加载完毕");

    env->SetObjectField(loadedApk,mClassLoaderField,dexClassLoader);

    LOGE("已用新的DexClassLoader替换默认的PathClassLoader");
}



JNIEXPORT void JNICALL
Java_dev_mars_secure_ProxyApplication_onShellCreate(JNIEnv *env, jobject instance,jint build_version) {
    //首先得到原Application的类名
    jclass ProxyApplicationClass = env->GetObjectClass(instance);
    jmethodID getPackageManager = env->GetMethodID(ProxyApplicationClass,"getPackageManager","()Landroid/content/pm/PackageManager;");
    jobject packageManager = env->CallObjectMethod(instance,getPackageManager);
    jmethodID pmGetApplicationInfo = env->GetMethodID(env->GetObjectClass(packageManager),"getApplicationInfo","(Ljava/lang/String;I)Landroid/content/pm/ApplicationInfo;");
    jmethodID getPackageName = env->GetMethodID(ProxyApplicationClass,"getPackageName","()Ljava/lang/String;");
    jobject pmAppInfo = env->CallObjectMethod(packageManager,pmGetApplicationInfo,env->CallObjectMethod(instance,getPackageName),128);

    jclass PackageItemInfoClass = env->FindClass("android/content/pm/PackageItemInfo");
    jfieldID metaDataField = env->GetFieldID(PackageItemInfoClass,"metaData","Landroid/os/Bundle;");
    jobject metaData = env->GetObjectField(pmAppInfo,metaDataField);
    if(metaData==NULL){
        LOGE("未找到Bundle");
        return;
    }
    jmethodID bundleGetString = env->GetMethodID(env->GetObjectClass(metaData),"getString","(Ljava/lang/String;)Ljava/lang/String;");
    jstring originApplicationName = (jstring) env->CallObjectMethod(metaData, bundleGetString, env->NewStringUTF("APP_NAME"));
    if(originApplicationName==NULL){
        LOGE("未找到原始Application Name");
        return;
    }
    LOGE("原始Application Name : %s",env->GetStringUTFChars(originApplicationName,false));
    //至此以得到原Application的类名

    //将LoadedApk中的mApplication对象替换
    jclass ActivityThreadClass = env->FindClass("android/app/ActivityThread");
    jmethodID currentActivityThread=env->GetStaticMethodID(ActivityThreadClass,"currentActivityThread","()Landroid/app/ActivityThread;");
    jobject activityThread = env->CallStaticObjectMethod(ActivityThreadClass,currentActivityThread);
    LOGE("得到 ActivityThreadClass");
    //得到AppBindData
    jfieldID mBoundApplicationField = env->GetFieldID(ActivityThreadClass,"mBoundApplication","Landroid/app/ActivityThread$AppBindData;");
    jobject mBoundApplication = env->GetObjectField(activityThread,mBoundApplicationField);
    LOGE("得到 AppBindData");
    //得到LoadedApk
    jfieldID infoField = env->GetFieldID(env->GetObjectClass(mBoundApplication),"info","Landroid/app/LoadedApk;");
    jobject info = env->GetObjectField(mBoundApplication,infoField);
    LOGE("得到 LoadedApk");
    //把LoadedApk中的成员变量private Application mApplication;置空
    jfieldID mApplicationField = env->GetFieldID(env->GetObjectClass(info),"mApplication","Landroid/app/Application;");
    env->SetObjectField(info,mApplicationField,NULL);
    LOGE("mApplication;置空");
    //得到壳Application
    jfieldID mInitialApplicationField = env->GetFieldID(ActivityThreadClass,"mInitialApplication","Landroid/app/Application;");
    jobject mInitialApplication = env->GetObjectField(activityThread,mInitialApplicationField);
    LOGE("得到壳Application");


    //将壳Application移除
    jfieldID mAllApplicationsField = env->GetFieldID(ActivityThreadClass,"mAllApplications","Ljava/util/ArrayList;");
    jobject mAllApplications = env->GetObjectField(activityThread,mAllApplicationsField);
    jmethodID remove = env->GetMethodID(env->GetObjectClass(mAllApplications),"remove","(Ljava/lang/Object;)Z");
    env->CallBooleanMethod(mAllApplications,remove,mInitialApplication);
    LOGE("将壳Application移除");
    //得到AppBindData中的ApplicationInfo
    jfieldID appInfoField = env->GetFieldID(env->GetObjectClass(mBoundApplication),"appInfo","Landroid/content/pm/ApplicationInfo;");
    jobject appInfo = env->GetObjectField(mBoundApplication,appInfoField);
    LOGE("得到AppBindData中的ApplicationInfo");
    //得到LoadedApk中的ApplicationInfo
    jfieldID mApplicationInfoField = env->GetFieldID(env->GetObjectClass(info),"mApplicationInfo","Landroid/content/pm/ApplicationInfo;");
    jobject mApplicationInfo = env->GetObjectField(info,mApplicationInfoField);
    LOGE("得到LoadedApk中的ApplicationInfo");
    //替换掉ApplicationInfo中的className
    jfieldID classNameField = env->GetFieldID(env->GetObjectClass(appInfo),"className","Ljava/lang/String;");
    env->SetObjectField(appInfo,classNameField,originApplicationName);
    env->SetObjectField(mApplicationInfo,classNameField,originApplicationName);
    LOGE("替换掉ApplicationInfo中的className");
    //创建新的Application
    jmethodID makeApplication = env->GetMethodID(env->GetObjectClass(info),"makeApplication","(ZLandroid/app/Instrumentation;)Landroid/app/Application;");
    jobject originApp = env->CallObjectMethod(info,makeApplication,false,NULL);
    LOGE("创建新的Application");
    //将句柄赋值到mInitialApplicationField
    env->SetObjectField(activityThread,mInitialApplicationField,originApp);
    LOGE("将句柄赋值到mInitialApplicationField");
    jfieldID mProviderMapField;
    if(build_version<19){
        mProviderMapField = env->GetFieldID(ActivityThreadClass,"mProviderMap","Ljava/util/HashMap;");
    }else{
        mProviderMapField = env->GetFieldID(ActivityThreadClass,"mProviderMap","Landroid/util/ArrayMap;");
    }
    if(mProviderMapField==NULL){
        LOGE("未找到mProviderMapField");
        return;
    }
    LOGE("找到mProviderMapField");
    jobject mProviderMap = env->GetObjectField(activityThread,mProviderMapField);
    LOGE("得到mProviderMap");
    jmethodID values = env->GetMethodID(env->GetObjectClass(mProviderMap),"values","()Ljava/util/Collection;");
    jobject collections = env->CallObjectMethod(mProviderMap,values);
    jmethodID iterator = env->GetMethodID(env->GetObjectClass(collections),"iterator","()Ljava/util/Iterator;");
    jobject mIterator  = env->CallObjectMethod(collections,iterator);
    jmethodID hasNext = env->GetMethodID(env->GetObjectClass(mIterator),"hasNext","()Z");
    jmethodID next = env->GetMethodID(env->GetObjectClass(mIterator),"next","()Ljava/lang/Object;");

    //替换所有ContentProvider中的Context
    LOGE("准备替换所有ContentProvider中的Context");
    while(env->CallBooleanMethod(mIterator,hasNext)){
        jobject providerClientRecord = env->CallObjectMethod(mIterator,next);
        if(providerClientRecord==NULL){
            LOGE("providerClientRecord = NULL");
            continue;
        }
        jclass ProviderClientRecordClass = env->FindClass("android/app/ActivityThread$ProviderClientRecord");
        jfieldID mLocalProviderField = env->GetFieldID(ProviderClientRecordClass,"mLocalProvider","Landroid/content/ContentProvider;");
        if(mLocalProviderField==NULL){
            LOGE("mLocalProviderField not found");
            continue;
        }
        jobject mLocalProvider = env->GetObjectField(providerClientRecord,mLocalProviderField);
        if(mLocalProvider==NULL){
            LOGE("mLocalProvider is NULL");
            continue;
        }
        jfieldID mContextField = env->GetFieldID(env->GetObjectClass(mLocalProvider),"mContext","Landroid/content/Context;");
        if(mContextField==NULL){
            LOGE("mContextField not found");
            continue;
        }
        env->SetObjectField(mLocalProvider,mContextField,originApp);
    }

    //执行originApp的onCreate
    LOGE("已完成脱壳");
    jmethodID onCreate = env->GetMethodID(env->GetObjectClass(originApp),"onCreate","()V");
    env->CallVoidMethod(originApp,onCreate);
    LOGE("壳Application执行完毕");
}

}




