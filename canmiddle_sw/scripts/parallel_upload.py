from platformio import util
from threading import Thread


Import('env')
UPLOADCMD = env['UPLOADCMD']


def _config_option(env, name, default=None):
    config = env.GetProjectConfig()
    try:
        return config.get('env:' + env['PIOENV'], name)
    except Exception:
        return default


def _serial_ports(filter_hwid=True):
    return [port['port'] for port in util.get_serial_ports(filter_hwid)]


def _upload(source, target, port, retries):
    for run in range(1 + retries):
        env.Replace(UPLOAD_PORT=port)
        command = env.subst(UPLOADCMD, source=source, target=target)
        retcode = env.Execute(command)
        if retcode == 0:
            break


def on_upload(source, target, env):
    upload_ports = _config_option(env, 'UPLOAD_PORTS')
    upload_retries = _config_option(env, 'UPLOAD_RETRIES', 3)
    serial_ports = _serial_ports()

    threads = set()
    for port in (upload_ports or serial_ports):
        kwargs = dict(source=source, target=target,
                      port=port, retries=upload_retries)
        thread = Thread(target=_upload, kwargs=kwargs)
        threads.add(thread)
        thread.start()
    for thread in threads:
        thread.join()


env.Replace(UPLOADCMD=on_upload)