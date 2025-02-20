#include <stdio.h>
#include <stdlib.h>
#include "c_kzg_4844_jni.h"
#include "c_kzg_4844.h"

static const char *TRUSTED_SETUP_NOT_LOADED = "Trusted Setup is not loaded.";

KZGSettings *settings;

void reset_trusted_setup()
{
  free(settings);
  settings = NULL;
}

void throw_exception(JNIEnv *env, const char *message)
{
  jclass exception_class = (*env)->FindClass(env, "java/lang/RuntimeException");
  (*env)->ThrowNew(env, exception_class, message);
}

void throw_c_kzg_exception(JNIEnv *env, C_KZG_RET error_code, const char *message)
{
  jclass exception_class = (*env)->FindClass(env, "ethereum/ckzg4844/CKZGException");
  jstring error_message = (*env)->NewStringUTF(env, message);
  jmethodID exception_init = (*env)->GetMethodID(env, exception_class, "<init>", "(ILjava/lang/String;)V");
  jobject exception = (*env)->NewObject(env, exception_class, exception_init, error_code, error_message);
  (*env)->Throw(env, exception);
}

JNIEXPORT jint JNICALL Java_ethereum_ckzg4844_CKZG4844JNI_getFieldElementsPerBlob(JNIEnv *env, jclass thisCls)
{
  return (jint)FIELD_ELEMENTS_PER_BLOB;
}

JNIEXPORT void JNICALL Java_ethereum_ckzg4844_CKZG4844JNI_loadTrustedSetup__Ljava_lang_String_2(JNIEnv *env, jclass thisCls, jstring file)
{
  if (settings != NULL)
  {
    throw_exception(env, "Trusted Setup is already loaded. Free it before loading a new one.");
    return;
  }
  settings = malloc(sizeof(KZGSettings));

  const char *file_native = (*env)->GetStringUTFChars(env, file, 0);

  FILE *f = fopen(file_native, "r");

  if (f == NULL)
  {
    reset_trusted_setup();
    (*env)->ReleaseStringUTFChars(env, file, file_native);
    throw_exception(env, "Couldn't load Trusted Setup. File might not exist or there is a permission issue.");
    return;
  }

  C_KZG_RET ret = load_trusted_setup_file(settings, f);

  if (ret != C_KZG_OK)
  {
    reset_trusted_setup();
    (*env)->ReleaseStringUTFChars(env, file, file_native);
    fclose(f);
    throw_c_kzg_exception(env, ret, "There was an error while loading the Trusted Setup.");
    return;
  }

  fclose(f);

  (*env)->ReleaseStringUTFChars(env, file, file_native);
}

JNIEXPORT void JNICALL Java_ethereum_ckzg4844_CKZG4844JNI_loadTrustedSetup___3BJ_3BJ(JNIEnv *env, jclass thisCls, jbyteArray g1, jlong g1Count, jbyteArray g2, jlong g2Count)
{
  if (settings != NULL)
  {
    throw_exception(env, "Trusted Setup is already loaded. Free it before loading a new one.");
    return;
  }
  settings = malloc(sizeof(KZGSettings));

  uint8_t *g1_native = (uint8_t *)(*env)->GetByteArrayElements(env, g1, NULL);
  uint8_t *g2_native = (uint8_t *)(*env)->GetByteArrayElements(env, g2, NULL);

  C_KZG_RET ret = load_trusted_setup(settings, g1_native, (size_t)g1Count, g2_native, (size_t)g2Count);

  if (ret != C_KZG_OK)
  {
    reset_trusted_setup();
    throw_c_kzg_exception(env, ret, "There was an error while loading the Trusted Setup.");
    return;
  }
}

JNIEXPORT void JNICALL Java_ethereum_ckzg4844_CKZG4844JNI_freeTrustedSetup(JNIEnv *env, jclass thisCls)
{
  if (settings == NULL)
  {
    throw_exception(env, TRUSTED_SETUP_NOT_LOADED);
    return;
  }
  free_trusted_setup(settings);
  reset_trusted_setup();
}

JNIEXPORT jbyteArray JNICALL Java_ethereum_ckzg4844_CKZG4844JNI_computeAggregateKzgProof(JNIEnv *env, jclass thisCls, jbyteArray blobs, jlong count)
{
  if (settings == NULL)
  {
    throw_exception(env, TRUSTED_SETUP_NOT_LOADED);
    return NULL;
  }

  size_t blobs_size = (size_t)(*env)->GetArrayLength(env, blobs);
  if (blobs_size == 0)
  {
    throw_exception(env, "Passing byte array with 0 elements for blobs is not supported.");
    return 0;
  }

  jbyte *blobs_native = (*env)->GetByteArrayElements(env, blobs, NULL);

  KZGProof p;

  C_KZG_RET ret = compute_aggregate_kzg_proof(&p, (uint8_t const(*)[BYTES_PER_BLOB])blobs_native, (size_t)count, settings);

  if (ret != C_KZG_OK)
  {
    throw_c_kzg_exception(env, ret, "There was an error while computing aggregate kzg proof.");
    return NULL;
  }

  jbyteArray proof = (*env)->NewByteArray(env, BYTES_PER_PROOF);
  uint8_t *out = (uint8_t *)(*env)->GetByteArrayElements(env, proof, 0);

  bytes_from_g1(out, &p);

  (*env)->ReleaseByteArrayElements(env, proof, (jbyte *)out, 0);

  return proof;
}

JNIEXPORT jboolean JNICALL Java_ethereum_ckzg4844_CKZG4844JNI_verifyAggregateKzgProof(JNIEnv *env, jclass thisCls, jbyteArray blobs, jbyteArray commitments, jlong count, jbyteArray proof)
{
  if (settings == NULL)
  {
    throw_exception(env, TRUSTED_SETUP_NOT_LOADED);
    return 0;
  }

  size_t blobs_size = (size_t)(*env)->GetArrayLength(env, blobs);
  if (blobs_size == 0)
  {
    throw_exception(env, "Passing byte array with 0 elements for blobs is not supported.");
    return 0;
  }

  jbyte *blobs_native = (*env)->GetByteArrayElements(env, blobs, NULL);
  uint8_t *commitments_native = (uint8_t *)(*env)->GetByteArrayElements(env, commitments, NULL);
  uint8_t *proof_native = (uint8_t *)(*env)->GetByteArrayElements(env, proof, NULL);
  size_t count_native = (size_t)count;

  KZGProof f;

  C_KZG_RET ret;

  ret = bytes_to_g1(&f, proof_native);

  if (ret != C_KZG_OK)
  {
    throw_c_kzg_exception(env, ret, "There was an error while converting proof to g1.");
    return 0;
  }

  KZGCommitment *c = calloc(count_native, sizeof(KZGCommitment));

  for (size_t i = 0; i < count_native; i++)
  {
    ret = bytes_to_g1(&c[i], &commitments_native[i * BYTES_PER_COMMITMENT]);
    if (ret != C_KZG_OK)
    {
      free(c);
      char arr[100];
      sprintf(arr, "There was an error while converting commitment (%zu/%zu) to g1.", i + 1, count_native);
      throw_c_kzg_exception(env, ret, arr);
      return 0;
    }
  }

  bool out;
  ret = verify_aggregate_kzg_proof(&out, (uint8_t const(*)[BYTES_PER_BLOB])blobs_native, c, count_native, &f, settings);

  if (ret != C_KZG_OK)
  {
    free(c);
    throw_c_kzg_exception(env, ret, "There was an error while verifying aggregate kzg proof.");
    return 0;
  }

  free(c);

  return (jboolean)out;
}

JNIEXPORT jbyteArray JNICALL Java_ethereum_ckzg4844_CKZG4844JNI_blobToKzgCommitment(JNIEnv *env, jclass thisCls, jbyteArray blob)
{
  if (settings == NULL)
  {
    throw_exception(env, TRUSTED_SETUP_NOT_LOADED);
    return NULL;
  }

  size_t blob_size = (size_t)(*env)->GetArrayLength(env, blob);
  if (blob_size == 0)
  {
    throw_exception(env, "Passing byte array with 0 elements for a blob is not supported.");
    return NULL;
  }

  jbyte *blob_native = (*env)->GetByteArrayElements(env, blob, NULL);

  KZGCommitment c;
  C_KZG_RET ret;

  ret = blob_to_kzg_commitment(&c, (uint8_t *)blob_native, settings);

  if (ret != C_KZG_OK)
  {
    throw_c_kzg_exception(env, ret, "There was an error while converting blob to commitment.");
    return NULL;
  }

  jbyteArray commitment = (*env)->NewByteArray(env, BYTES_PER_COMMITMENT);
  uint8_t *out = (uint8_t *)(*env)->GetByteArrayElements(env, commitment, 0);

  bytes_from_g1(out, &c);

  (*env)->ReleaseByteArrayElements(env, commitment, (jbyte *)out, 0);

  return commitment;
}

JNIEXPORT jboolean JNICALL Java_ethereum_ckzg4844_CKZG4844JNI_verifyKzgProof(JNIEnv *env, jclass thisCls, jbyteArray commitment, jbyteArray z, jbyteArray y, jbyteArray proof)
{
  if (settings == NULL)
  {
    throw_exception(env, TRUSTED_SETUP_NOT_LOADED);
    return 0;
  }

  uint8_t *commitment_native = (uint8_t *)(*env)->GetByteArrayElements(env, commitment, NULL);
  uint8_t *z_native = (uint8_t *)(*env)->GetByteArrayElements(env, z, NULL);
  uint8_t *y_native = (uint8_t *)(*env)->GetByteArrayElements(env, y, NULL);
  uint8_t *proof_native = (uint8_t *)(*env)->GetByteArrayElements(env, proof, NULL);

  KZGCommitment c;
  KZGProof p;
  bool out;

  C_KZG_RET ret;

  ret = bytes_to_g1(&c, commitment_native);

  if (ret != C_KZG_OK)
  {
    throw_c_kzg_exception(env, ret, "There was an error while converting commitment to g1.");
    return 0;
  }

  ret = bytes_to_g1(&p, proof_native);

  if (ret != C_KZG_OK)
  {
    throw_c_kzg_exception(env, ret, "There was an error while converting proof to g1.");
    return 0;
  }

  ret = verify_kzg_proof(&out, &c, z_native, y_native, &p, settings);

  if (ret != C_KZG_OK)
  {
    throw_c_kzg_exception(env, ret, "There was an error while verifying kzg proof.");
    return 0;
  }

  return (jboolean)out;
}
