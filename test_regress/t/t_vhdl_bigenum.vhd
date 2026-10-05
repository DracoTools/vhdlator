-- DESCRIPTION: Verilator: VHDL 'VALUE and 'IMAGE of an enumeration type with
-- more literals than fit one nested choice
--
-- An enumeration of three hundred literals reads and writes its values by
-- name like any other (16.2.3): 'VALUE of a string known only as the design
-- runs, and 'IMAGE of a value only the run decides (GHDL gna bug7751).
--
-- This file ONLY is placed under the Creative Commons Public Domain.
-- SPDX-FileCopyrightText: 2026 Wilson Snyder
-- SPDX-License-Identifier: CC0-1.0

entity t_vhdl_bigenum is
end entity;

architecture a of t_vhdl_bigenum is
  type e300 is (
    T0, T1, T2, T3, T4, T5, T6, T7, T8, T9,
    T10, T11, T12, T13, T14, T15, T16, T17, T18, T19,
    T20, T21, T22, T23, T24, T25, T26, T27, T28, T29,
    T30, T31, T32, T33, T34, T35, T36, T37, T38, T39,
    T40, T41, T42, T43, T44, T45, T46, T47, T48, T49,
    T50, T51, T52, T53, T54, T55, T56, T57, T58, T59,
    T60, T61, T62, T63, T64, T65, T66, T67, T68, T69,
    T70, T71, T72, T73, T74, T75, T76, T77, T78, T79,
    T80, T81, T82, T83, T84, T85, T86, T87, T88, T89,
    T90, T91, T92, T93, T94, T95, T96, T97, T98, T99,
    T100, T101, T102, T103, T104, T105, T106, T107, T108, T109,
    T110, T111, T112, T113, T114, T115, T116, T117, T118, T119,
    T120, T121, T122, T123, T124, T125, T126, T127, T128, T129,
    T130, T131, T132, T133, T134, T135, T136, T137, T138, T139,
    T140, T141, T142, T143, T144, T145, T146, T147, T148, T149,
    T150, T151, T152, T153, T154, T155, T156, T157, T158, T159,
    T160, T161, T162, T163, T164, T165, T166, T167, T168, T169,
    T170, T171, T172, T173, T174, T175, T176, T177, T178, T179,
    T180, T181, T182, T183, T184, T185, T186, T187, T188, T189,
    T190, T191, T192, T193, T194, T195, T196, T197, T198, T199,
    T200, T201, T202, T203, T204, T205, T206, T207, T208, T209,
    T210, T211, T212, T213, T214, T215, T216, T217, T218, T219,
    T220, T221, T222, T223, T224, T225, T226, T227, T228, T229,
    T230, T231, T232, T233, T234, T235, T236, T237, T238, T239,
    T240, T241, T242, T243, T244, T245, T246, T247, T248, T249,
    T250, T251, T252, T253, T254, T255, T256, T257, T258, T259,
    T260, T261, T262, T263, T264, T265, T266, T267, T268, T269,
    T270, T271, T272, T273, T274, T275, T276, T277, T278, T279,
    T280, T281, T282, T283, T284, T285, T286, T287, T288, T289,
    T290, T291, T292, T293, T294, T295, T296, T297, T298, T299);
  type e4 is (one, two, three, four);
  signal name_s : string(1 to 4) := "T257";
  signal small_s : string(1 to 5) := "THREE";
  signal pos_s : natural := 299;

  function e_val(s : string) return e300 is
  begin
    return e300'value(s);
  end function;
begin
  process is
    variable v : e300;
  begin
    wait for 1 ns;
    v := e_val(name_s);
    assert v = T257 severity failure;
    assert e300'pos(v) = 257 severity failure;
    assert e300'value(" t42 ") = T42 severity failure;
    assert e4'value(small_s) = three severity failure;
    assert e300'image(e300'val(pos_s)) = "t299" severity failure;
    assert e300'image(v) = "t257" severity failure;
    v := e300'val(pos_s - 299);
    assert e300'image(v) = "t0" severity failure;
    report "*-* All Finished *-*";
    wait;
  end process;
end architecture;
