#include <openssl/err.h>
#include <openssl/ossl_typ.h>
#include <openssl/ui.h>
#include <openssl/pem.h>
#include <openssl/rand.h>

#include "apps.h"
#define DEFBITS	512

static UI_METHOD *ui_method = NULL;

static int ui_open(UI *ui)
{
	return UI_method_get_opener(UI_OpenSSL())(ui);
}

static int ui_read(UI *ui, UI_STRING *uis)
{
	if (UI_get_input_flags(uis) & UI_INPUT_FLAG_DEFAULT_PWD
		&& UI_get0_user_data(ui))
	{
		switch(UI_get_string_type(uis))
		{
		case UIT_PROMPT:
		case UIT_VERIFY:
			{
				const char *password = (const char*)
					((PW_CB_DATA *)UI_get0_user_data(ui))->password;
				if (password && password[0] != '\0')
				{
					UI_set_result(ui, uis, password);
					return 1;
				}
			}
		default:
			break;
		}
	}
	return UI_method_get_reader(UI_OpenSSL())(ui, uis);
}

static int ui_write(UI *ui, UI_STRING *uis)
{
	if (UI_get_input_flags(uis) & UI_INPUT_FLAG_DEFAULT_PWD
		&& UI_get0_user_data(ui))
	{
		switch(UI_get_string_type(uis))
		{
		case UIT_PROMPT:
		case UIT_VERIFY:
			{
				const char *password = (const char*)
					((PW_CB_DATA *)UI_get0_user_data(ui))->password;
				if (password && password[0] != '\0')
					return 1;
			}
		default:
			break;
		}
	}
	return UI_method_get_writer(UI_OpenSSL())(ui, uis);
}

static int ui_close(UI *ui)
{
	return UI_method_get_closer(UI_OpenSSL())(ui);
}

int setup_ui_method(void)
{
	ui_method = UI_create_method("OpenSSL application user interface");
	UI_method_set_opener(ui_method, ui_open);
	UI_method_set_reader(ui_method, ui_read);
	UI_method_set_writer(ui_method, ui_write);
	UI_method_set_closer(ui_method, ui_close);
	return 0;
}

static void destroy_ui_method(void)
	{
	if(ui_method)
		{
		UI_destroy_method(ui_method);
		ui_method = NULL;
		}
	}
static int password_callback(char *buf, int bufsiz, int verify,
	PW_CB_DATA *cb_tmp)
	{
	UI *ui = NULL;
	int res = 0;
	const char *prompt_info = NULL;
	const char *password = NULL;
	PW_CB_DATA *cb_data = (PW_CB_DATA *)cb_tmp;

	if (cb_data)
		{
		if (cb_data->password)
			password = cb_data->password;
		if (cb_data->prompt_info)
			prompt_info = cb_data->prompt_info;
		}

	if (password)
		{
		res = strlen(password);
		if (res > bufsiz)
			res = bufsiz;
		memcpy(buf, password, res);
		return res;
		}

	ui = UI_new_method(ui_method);
	if (ui)
		{
		int ok = 0;
		char *buff = NULL;
		int ui_flags = 0;
		char *prompt = NULL;

		prompt = UI_construct_prompt(ui, "pass phrase",
			prompt_info);

		ui_flags |= UI_INPUT_FLAG_DEFAULT_PWD;
		UI_ctrl(ui, UI_CTRL_PRINT_ERRORS, 1, 0, 0);

		if (ok >= 0)
			ok = UI_add_input_string(ui,prompt,ui_flags,buf,
				PW_MIN_LENGTH,BUFSIZ-1);
		if (ok >= 0 && verify)
			{
			buff = (char *)OPENSSL_malloc(bufsiz);
			ok = UI_add_verify_string(ui,prompt,ui_flags,buff,
				PW_MIN_LENGTH,BUFSIZ-1, buf);
			}
		if (ok >= 0)
			do
				{
				ok = UI_process(ui);
				}
			while (ok < 0 && UI_ctrl(ui, UI_CTRL_IS_REDOABLE, 0, 0, 0));

		if (buff)
			{
			OPENSSL_cleanse(buff,(unsigned int)bufsiz);
			OPENSSL_free(buff);
			}

		if (ok >= 0)
			res = strlen(buf);
		if (ok == -1)
			{
			BIO_printf(bio_err, "User interface error\n");
			ERR_print_errors(bio_err);
			OPENSSL_cleanse(buf,(unsigned int)bufsiz);
			res = 0;
			}
		if (ok == -2)
			{
			BIO_printf(bio_err,"aborted!\n");
			OPENSSL_cleanse(buf,(unsigned int)bufsiz);
			res = 0;
			}
		UI_free(ui);
		OPENSSL_free(prompt);
		}
	return res;
}

static int load_config(BIO *err, CONF *cnf)
{
	if (!cnf)
		cnf = config;
	if (!cnf)
		return 1;

	OPENSSL_load_builtin_modules();

	if (CONF_modules_load(cnf, NULL, 0) <= 0)
		{
		BIO_printf(err, "Error configuring OpenSSL\n");
		ERR_print_errors(err);
		return 0;
		}
	return 1;
}

static int MS_CALLBACK genrsa_cb(int p, int n, BN_GENCB *cb)
{
	char c='*';

	if (p == 0) c='.';
	if (p == 1) c='+';
	if (p == 2) c='*';
	if (p == 3) c='\n';
	BIO_write((BIO*)cb->arg,&c,1);
	(void)BIO_flush((BIO*)cb->arg);
#ifdef LINT
	p=n;
#endif
	return 1;
}

static EVP_PKEY *load_key(BIO *err, const char *file, int format, int maybe_stdin,
	const char *pass, ENGINE *e, const char *key_descrip)
{
	BIO *key=NULL;
	EVP_PKEY *pkey=NULL;
	PW_CB_DATA cb_data;

	cb_data.password = pass;
	cb_data.prompt_info = file;

	if (file == NULL && (!maybe_stdin || format == FORMAT_ENGINE))
	{
		BIO_printf(err,"no keyfile specified\n");
		goto end;
	}
#ifndef OPENSSL_NO_ENGINE
	if (format == FORMAT_ENGINE)
		{
		if (!e)
			BIO_printf(bio_err,"no engine specified\n");
		else
			pkey = ENGINE_load_private_key(e, file,
				ui_method, &cb_data);
		goto end;
		}
#endif
	key=BIO_new(BIO_s_file());
	if (key == NULL)
	{
		ERR_print_errors(err);
		goto end;
	}
	if (file == NULL && maybe_stdin)
	{
		setvbuf(stdin, NULL, _IONBF, 0);
		BIO_set_fp(key,stdin,BIO_NOCLOSE);
	}
	else if (BIO_read_filename(key,file) <= 0)
	{
		BIO_printf(err, "Error opening %s %s\n", key_descrip, file);
		ERR_print_errors(err);
		goto end;
	}
	if (format == FORMAT_ASN1)
	{
		pkey=d2i_PrivateKey_bio(key, NULL);
	}
	else if (format == FORMAT_PEM)
	{
		pkey=PEM_read_bio_PrivateKey(key,NULL, (pem_password_cb *)password_callback, &cb_data);
	}
#if defined(PACS_USING_FORMAT_NETSCAPE_OR_PKCS12)
#if !defined(OPENSSL_NO_RC4) && !defined(OPENSSL_NO_RSA)
	else if (format == FORMAT_NETSCAPE || format == FORMAT_IISSGC)
		pkey = load_netscape_key(err, key, file, key_descrip, format);
#endif
	else if (format == FORMAT_PKCS12)
	{
		if (!load_pkcs12(err, key, key_descrip,
				(pem_password_cb *)password_callback, &cb_data,
				&pkey, NULL, NULL))
			goto end;
	}
	else
	{
		BIO_printf(err,"bad input format specified for key file\n");
		goto end;
	}
#endif
 end:
	if (key != NULL) BIO_free(key);
	if (pkey == NULL)
		BIO_printf(err,"unable to load %s\n", key_descrip);
	return(pkey);
}

static RSA * loadCheckPrivateKey(char *privateKey)
{
	RSA *rsaLoadPrivate = NULL;
	{
		EVP_PKEY *pkey = load_key(bio_err, privateKey, FORMAT_PEM, 1, NULL, NULL, "Private Key");
		if (pkey != NULL)
			rsaLoadPrivate = pkey == NULL ? NULL : EVP_PKEY_get1_RSA(pkey);
		EVP_PKEY_free(pkey);
	}

	if (rsaLoadPrivate == NULL)
	{
		ERR_print_errors(bio_err);
		return NULL;
	}
	else
	{
		//check loaded private key
		int r = RSA_check_key(rsaLoadPrivate);
		if (r == 1)
			BIO_printf(bio_err, "RSA private key check ok\n");
		else if (r == 0)
		{
			unsigned long err;
			while ((err = ERR_peek_error()) != 0 &&
				ERR_GET_LIB(err) == ERR_LIB_RSA &&
				ERR_GET_FUNC(err) == RSA_F_RSA_CHECK_KEY &&
				ERR_GET_REASON(err) != ERR_R_MALLOC_FAILURE)
			{
				BIO_printf(bio_err, "RSA private key error: %s\n", ERR_reason_error_string(err));
				ERR_get_error(); // remove e from error stack
			}
		}

		if (r == -1 || ERR_peek_error() != 0) // should happen only if r == -1
		{
			ERR_print_errors(bio_err);
			return NULL;
		}
	}
	return rsaLoadPrivate;
}

int genrsa(int num, char *privateKey, char *publicKey)
{
	char *passargout = NULL, *passout = NULL;
	unsigned long f4 = RSA_F4;
	long l = 0L;
	int i, ret = 1;
	BN_GENCB cb;
	BIGNUM *bn = BN_new();
	RSA *rsa = RSA_new();
	BIO *out = NULL;

	apps_startup();
	BN_GENCB_set(&cb, genrsa_cb, bio_err);
	if (bio_err == NULL)
		if ((bio_err=BIO_new(BIO_s_file())) != NULL)
			BIO_set_fp(bio_err, stderr, BIO_NOCLOSE|BIO_FP_TEXT);

	if (!load_config(bio_err, NULL))
		goto err;
	if ((out = BIO_new(BIO_s_file())) == NULL)
	{
		BIO_printf(bio_err,"unable to create BIO for output\n");
		goto err;
	}

	if (privateKey == NULL)
	{
		BIO_set_fp(out, stdout, BIO_NOCLOSE);
	}
	else
	{
		if (BIO_write_filename(out, privateKey) <= 0)
		{
			perror(privateKey);
			goto err;
		}
	}
	if (!app_RAND_load_file(NULL, bio_err, 1) && !RAND_status())
		BIO_printf(bio_err,"warning, not much extra random data, consider using the -rand option\n");
	BIO_printf(bio_err,"Generating RSA private key, %d bit long modulus\n",	num);

	BN_set_word(bn, f4);
	RSA_generate_key_ex(rsa, num, bn, &cb);
	app_RAND_write_file(NULL, bio_err);

	for (i = 0; i < rsa->e->top; ++i)
	{
#ifndef SIXTY_FOUR_BIT
		l <<= BN_BITS4;
		l <<= BN_BITS4;
#endif
		l += rsa->e->d[i];
	}
	BIO_printf(bio_err,"e is %ld (0x%lX)\nwriting RSA private key\n",l,l);
	{
		PW_CB_DATA cb_data;
		cb_data.password = passout;
		cb_data.prompt_info = privateKey;
		if (!PEM_write_bio_RSAPrivateKey(out, rsa, NULL, NULL, 0, (pem_password_cb *)password_callback, &cb_data))
			goto err;
	}
	BIO_free_all(out); // flush out, complete writing private key
	out = NULL;
	BIO_printf(bio_err,"RSA private key OK\n");

	// private key OK, export public key
	/*
	rsaLoadPrivate = loadCheckPrivateKey(privateKey);
	if(rsaLoadPrivate == NULL)
	{
		BIO_printf(bio_err,"loading and checking RSA private key failed\n");
		goto err;
	}
	*/
	// set output to public key file
	out = BIO_new(BIO_s_file());
	if (BIO_write_filename(out, publicKey) <= 0)
	{
		perror(publicKey);
		goto err;
	}
	BIO_printf(bio_err,"writing RSA public key\n");
	i = PEM_write_bio_RSA_PUBKEY(out, rsa);
	if (!i)
	{
		BIO_printf(bio_err,"unable to write key\n");
		ERR_print_errors(bio_err);
		goto err;
	}
	BIO_printf(bio_err,"RSA public key OK\n");
	ret=0;
err:
	if (bn) BN_free(bn);
	if (rsa) RSA_free(rsa);
	if (out) BIO_free_all(out);
	if(passout) OPENSSL_free(passout);
	if (ret != 0)
		ERR_print_errors(bio_err);
	apps_shutdown();
	return ret;
}