    prompt 0
    nx 4
    ny 4
    nz 4
    nt 8

   verbose_flag 0
   reload_serial  ../binary_samples/lat.sample.l4448

   nkap_spectator  1
   max_cg_iterations 100
   max_cg_restarts 10
   error_for_propagator 1e-5
   kappa_spectator  0.120
   gaussian
   r0 1.0
   reload_serial_wprop  ../binary_samples/lprop.wi.sample.l4448

   nkap_light_zonked 1
   max_cg_iterations 100
   max_cg_restarts 10
   error_for_propagator 1e-5
   kappa_zonked_light 0.120
   gaussian
   r0 1.0
   reload_serial_wprop  ../binary_samples/lprop.wi.sample.l4448
   save_serial_wprop    _ssink

   nkap_heavy_zonked 1
   max_cg_iterations 100
   max_cg_restarts 10
   error_for_propagator 1e-5
   kappa_zonked_heavy 0.120
   hopilu
   gaussian
   r0 1.0
   save_serial_wprop  hprop.wi.test.l4448
   save_serial_wprop    _ssink

   nkap_sequential 1
   kappa_seq  0.120
   hopilu

   final_time 3 
start_of_momentum p
0 0 0
end_momentum
start_of_momentum q
0 0 0
end_momentum
start_of_momentum k
0 0 0
end_momentum
   seq_smear_func  ../binary_samples/in.sample.local_smear
   save_ascii   HH3      out.test.HH3_l4448.w_mr_prop_form.x
   save_ascii   HL3      out.test.HL3_l4448.w_mr_prop_form.x
   save_ascii   HH2_GL   out.test.HH2_GL_l4448.w_mr_prop_form.x
   save_ascii   LL2_GG   out.test.LL2_GG_l4448.w_mr_prop_form.x
   save_ascii   HL2_GG   out.test.HL2_GG_l4448.w_mr_prop_form.x
   save_ascii   HL2_GE   out.test.HL2_GE_l4448.w_mr_prop_form.x
   save_ascii   HL2_GL   out.test.HL2_GL_l4448.w_mr_prop_form.x
