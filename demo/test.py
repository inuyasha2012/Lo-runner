#!/usr/bin/python
# ! -*- coding: utf8 -*-

import lorun
import os
from signal import alarm, signal, SIGALRM, SIGKILL
from subprocess import PIPE, Popen

RESULT_STR = [
    'Accepted',
    'Presentation Error',
    'Time Limit Exceeded',
    'Memory Limit Exceeded',
    'Wrong Answer',
    'Runtime Error',
    'Output Limit Exceeded',
    'Compile Error',
    'System Error'
]


def run(args, cwd=None, shell=False, kill_tree=True, timeout=-1, env=None):
    '''
    Run a command with a timeout after which it will be forcibly
    killed.
    '''

    class Alarm(Exception):
        pass

    def alarm_handler(signum, frame):
        raise Alarm

    p = Popen(args, shell=shell, cwd=cwd, stdout=PIPE, stderr=PIPE, env=env)
    if timeout != -1:
        signal(SIGALRM, alarm_handler)
        alarm(timeout)
    try:
        stdout, stderr = p.communicate()
        if timeout != -1:
            alarm(0)
    except Alarm:
        pids = [p.pid]
        if kill_tree:
            pids.extend(get_process_children(p.pid))
        for pid in pids:
            # process might have died before getting to this line
            # so wrap to avoid OSError: no such process
            try:
                os.kill(pid, SIGKILL)
            except OSError:
                pass
        return -9, '', ''
    return p.returncode, stdout, stderr


def get_process_children(pid):
    p = Popen('ps --no-headers -o pid --ppid %d' % pid, shell=True,
              stdout=PIPE, stderr=PIPE)
    stdout, stderr = p.communicate()
    return [int(p) for p in stdout.split()]


def compile_src(src_path):
    if run('gcc %s -o m' % src_path, shell=True, timeout=3) != 0:
        print('compile failure!')
        return False
    return True


def runone(p_path, in_path, out_path):
    fin = open(in_path)
    ftemp = open('temp.out', 'w')

    runcfg = {
        'args': ['./m'],
        'fd_in': fin.fileno(),
        'fd_out': ftemp.fileno(),
        'timelimit': 1000,  # in MS
        'memorylimit': 20000,  # in KB
    }

    rst = lorun.run(runcfg)
    fin.close()
    ftemp.close()

    if rst['result'] == 0:
        ftemp = open('temp.out')
        fout = open(out_path)
        crst = lorun.check(fout.fileno(), ftemp.fileno())
        fout.close()
        ftemp.close()
        os.remove('temp.out')
        if crst != 0:
            return {'result': crst}
    return rst


def judge(src_path, td_path, td_total):
    if not compile_src(src_path):
        return
    for i in range(td_total):
        in_path = os.path.join(td_path, '%d.in' % i)
        out_path = os.path.join(td_path, '%d.out' % i)
        if os.path.isfile(in_path) and os.path.isfile(out_path):
            rst = runone('./m', in_path, out_path)
            rst['result'] = RESULT_STR[rst['result']]
            print(rst)
        else:
            print('testdata:%d incompleted' % i)
            os.remove('./m')
            exit(-1)
    os.remove('./m')


if __name__ == '__main__':
    import sys

    if len(sys.argv) != 4:
        print('Usage:%s srcfile testdata_pth testdata_total')
        exit(-1)
    judge('a+b.c', 'testdata', 3)
