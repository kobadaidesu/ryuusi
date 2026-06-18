/* ---- i18n ---- */
const i18n = {
  ja: {
    'about-heading': 'マウスとキーボードで<br />粒子をリアルタイムに操作',
    'about-desc': '最大30万個の粒子が生み出す視覚的な変化を楽しめるインタラクティブビジュアル作品です。',
    'fun-heading': '予測しきれない<br />動きと模様',
    'fun-desc': '渦・波紋・引力などを組み合わせることで、予測しきれない動きや模様が生まれます。パラメータを調整することで、変化の強さや印象も大きく変わり、自分だけのデジタルアートを見つける感覚を楽しめます。',
    'ops-heading': '粒子を操作する<br />主な５つの方法',
    'op1-title': '引力', 'op1-key': '左クリック',
    'op1-desc': 'カーソルに向かって粒子が引き寄せられる操作です。周囲の粒子が吸い込まれるように集まり、流れにまとまりが生まれます。',
    'op2-title': '渦', 'op2-key': 'V キー',
    'op2-desc': 'カーソル周辺に渦のような回転を生み出す操作です。粒子を巻き込みながら流れを変化させ、複雑なうねりを作り出します。',
    'op3-title': 'タイムストップ', 'op3-key': 'T キー',
    'op3-desc': '粒子を急激に減速させる操作です。動いていた粒子が一気に静まり、時間が止まったような印象的な変化を生み出します。',
    'op4-title': '波紋発生', 'op4-key': 'B キー',
    'op4-desc': '一点から衝撃波のような広がりを生み出す操作です。中心から外側へ向かって粒子が押し広げられ、波のような動きが発生します。',
    'op5-title': '収束', 'op5-key': 'G キー',
    'op5-desc': '画面上の粒子を一気に集める操作です。散らばっていた粒子がまとまり、密度の高いダイナミックな変化を楽しめます。',
    'ops-note': '※見やすいようにパラメーターを一部設定しています',
    'ops-other-label': 'その他の操作',
    'ops-other-restart': '再スタート', 'ops-other-explode': '爆発', 'ops-other-param': 'パラメータ設定画面',
    'params-heading': '詳細設定',
    'params-desc': '強弱、範囲、速度などのパラメータを調整できます。',
    'param1-title': '粒子の大きさ', 'param1-desc': '細かすぎるなと感じたときに使ってください。',
    'param2-title': '粒子の数', 'param2-desc': '多すぎて見づらいときに使ってください。',
    'param3-title': '色の変更', 'param3-desc': '好きな色に変えてみてください！',
    'params-more': 'さらに細かい設定もできます →',
    'rec-heading': 'おすすめの遊び方',
    'rec1-title': '渦で巻き込み、集める', 'rec1-subtitle': 'V ずっと長押し ⇒ G 押 ⇒ G 離',
    'rec1-desc': '渦を発生させたあとに粒子を集めると、流れが大きく変化します。参考映像を見ながら試してみてください。',
    'rec2-title': '一瞬の変化を楽しむ', 'rec2-subtitle': 'G を一瞬押して、E を長押し',
    'rec2-desc': '粒子の動きに大きな変化が生まれます。短い操作でも印象が大きく変わるのが特徴です。Gを押す長さによって見れるものが変わるかも、、、？ぎりぎりまでGを押して開放してみよう！',
    'rec3-title': '線のような表現を作る', 'rec3-subtitle': 'G を長押し後にカーソルを動かす',
    'rec3-desc': '線のような軌跡が現れます。その線に対して左クリックすると、また違った変化を楽しめます。',
    'rec4-title': 'パラメータを変えてみる', 'rec4-subtitle': 'speed limit を 200px/s に変更',
    'rec4-desc': 'params を開き、Physics → speed limit を 200px/s に変更して１，２，３をもう一度試してみてください。同じ操作でも、まったく違う光景が見えてくると思います。',
    'final-heading': '自分だけの動きや<br />表現を見つけてみてください',
    'final-desc': 'パラメータを1つ変えたり、ボタンの押す長さやタイミングで見える景色は大きく変わります。',
    'footer-rights': '© 2026 Particle Flow', 'footer-tagline': 'Designed with intention',
  },
  en: {
    'about-heading': 'Manipulate particles<br />in real time with mouse and keyboard',
    'about-desc': 'An interactive visual art piece where you can enjoy visual changes created by up to 300,000 particles.',
    'fun-heading': 'Unpredictable<br />movements and patterns',
    'fun-desc': 'By combining vortex, ripple, and gravity effects, unpredictable movements and patterns emerge. Adjusting the parameters dramatically changes the intensity and impression, letting you discover your own unique digital art.',
    'ops-heading': '5 main ways to<br />manipulate particles',
    'op1-title': 'Gravity', 'op1-key': 'Left click',
    'op1-desc': 'Particles are drawn toward the cursor. The surrounding particles gather as if being sucked in, creating a unified flow.',
    'op2-title': 'Vortex', 'op2-key': 'V key',
    'op2-desc': 'Creates a vortex-like rotation around the cursor. It draws in particles while changing the flow, creating complex undulations.',
    'op3-title': 'Time Stop', 'op3-key': 'T key',
    'op3-desc': 'Rapidly decelerates particles. Moving particles suddenly settle, creating an impressive change as if time has stopped.',
    'op4-title': 'Ripple', 'op4-key': 'B key',
    'op4-desc': 'Creates a shockwave-like spread from a single point. Particles are pushed outward from the center, generating wave-like movement.',
    'op5-title': 'Converge', 'op5-key': 'G key',
    'op5-desc': 'Gathers all particles on screen at once. Scattered particles come together, delivering a dense and dynamic change.',
    'ops-note': '※ Some parameters have been adjusted for better visibility',
    'ops-other-label': 'Other operations',
    'ops-other-restart': 'Restart', 'ops-other-explode': 'Explosion', 'ops-other-param': 'Parameter settings',
    'params-heading': 'Parameters',
    'params-desc': 'Adjust parameters such as intensity, range, and speed.',
    'param1-title': 'Particle size', 'param1-desc': 'Use this when particles feel too fine.',
    'param2-title': 'Particle count', 'param2-desc': 'Use this when too many particles make it hard to see.',
    'param3-title': 'Color change', 'param3-desc': 'Try changing to your favorite color!',
    'params-more': 'Even more detailed settings are available →',
    'rec-heading': 'Recommended ways to play',
    'rec1-title': 'Draw in and gather with vortex', 'rec1-subtitle': 'Hold V → Press G → Release G',
    'rec1-desc': 'After creating a vortex, gathering the particles dramatically changes the flow. Try it while watching the reference video.',
    'rec2-title': 'Enjoy a momentary change', 'rec2-subtitle': 'Press G briefly, then hold E',
    'rec2-desc': 'A big change occurs in particle movement. Even a short operation creates a dramatically different impression. Depending on how long you hold G, what you see may change? Push G to the limit and release!',
    'rec3-title': 'Create line-like expressions', 'rec3-subtitle': 'Hold G then move the cursor',
    'rec3-desc': 'A line-like trail appears. Left-clicking on that line creates yet another different change.',
    'rec4-title': 'Try changing parameters', 'rec4-subtitle': 'Change speed limit to 200px/s',
    'rec4-desc': 'Open params, go to Physics → speed limit and change it to 200px/s, then try steps 1, 2, and 3 again. The same operations will reveal a completely different scene.',
    'final-heading': 'Find your own<br />unique movement and expression',
    'final-desc': 'Changing just one parameter, or the duration and timing of button presses, dramatically changes what you see.',
    'footer-rights': '© 2026 Particle Flow', 'footer-tagline': 'Designed with intention',
  },
  zh: {
    'about-heading': '用鼠标和键盘<br />实时操控粒子',
    'about-desc': '这是一个交互式视觉艺术作品，您可以欣赏多达30万个粒子带来的视觉变化。',
    'fun-heading': '难以预测的<br />动态与图案',
    'fun-desc': '通过组合漩涡、涟漪、引力等效果，产生难以预测的动态与图案。调整参数可以大幅改变变化的强度和印象，让您享受发现专属数字艺术的感觉。',
    'ops-heading': '操控粒子的<br />5种主要方式',
    'op1-title': '引力', 'op1-key': '左键点击',
    'op1-desc': '粒子向光标方向被吸引。周围的粒子如被吸入般聚集，形成统一的流动。',
    'op2-title': '漩涡', 'op2-key': 'V 键',
    'op2-desc': '在光标周围产生漩涡般的旋转。卷入粒子的同时改变流动，创造复杂的波动。',
    'op3-title': '时间停止', 'op3-key': 'T 键',
    'op3-desc': '使粒子急剧减速。运动中的粒子瞬间平静，产生如时间停止般震撼的变化。',
    'op4-title': '涟漪发生', 'op4-key': 'B 键',
    'op4-desc': '从一点产生冲击波般的扩散。粒子从中心向外被推开，产生波浪般的运动。',
    'op5-title': '收束', 'op5-key': 'G 键',
    'op5-desc': '一次性收集屏幕上的所有粒子。分散的粒子聚集在一起，带来密度高的动态变化。',
    'ops-note': '※ 已对部分参数进行调整以便于观看',
    'ops-other-label': '其他操作',
    'ops-other-restart': '重新开始', 'ops-other-explode': '爆炸', 'ops-other-param': '参数设置界面',
    'params-heading': '详细设置',
    'params-desc': '可以调整强弱、范围、速度等参数。',
    'param1-title': '粒子大小', 'param1-desc': '当感觉粒子太细时使用。',
    'param2-title': '粒子数量', 'param2-desc': '当粒子太多难以看清时使用。',
    'param3-title': '颜色变更', 'param3-desc': '试着改成您喜欢的颜色！',
    'params-more': '还有更多详细设置可以调整 →',
    'rec-heading': '推荐玩法',
    'rec1-title': '用漩涡卷入并聚集', 'rec1-subtitle': '长按V ⇒ 按G ⇒ 松开G',
    'rec1-desc': '产生漩涡后聚集粒子，流动会大幅变化。请参考示例视频尝试。',
    'rec2-title': '享受瞬间的变化', 'rec2-subtitle': '短按G，然后长按E',
    'rec2-desc': '粒子的运动会产生巨大变化。短暂的操作也能带来截然不同的印象。按G的时长不同，看到的景象也可能不同？把G按到极限再松开试试！',
    'rec3-title': '创造线条般的表现', 'rec3-subtitle': '长按G后移动光标',
    'rec3-desc': '会出现线条般的轨迹。对那条线左键点击，可以享受另一种不同的变化。',
    'rec4-title': '尝试改变参数', 'rec4-subtitle': '将speed limit改为200px/s',
    'rec4-desc': '打开params，在Physics → speed limit中改为200px/s，然后再次尝试步骤1、2、3。相同的操作会呈现完全不同的景象。',
    'final-heading': '寻找属于自己的<br />独特动态与表现',
    'final-desc': '改变一个参数，或按钮按压的时长与时机，所看到的景象会大幅改变。',
    'footer-rights': '© 2026 Particle Flow', 'footer-tagline': 'Designed with intention',
  },
  ko: {
    'about-heading': '마우스와 키보드로<br />파티클을 실시간 조작',
    'about-desc': '최대 30만 개의 파티클이 만들어내는 시각적 변화를 즐길 수 있는 인터랙티브 비주얼 작품입니다.',
    'fun-heading': '예측할 수 없는<br />움직임과 패턴',
    'fun-desc': '소용돌이, 파문, 인력 등을 조합함으로써 예측할 수 없는 움직임과 패턴이 생겨납니다. 파라미터를 조정함으로써 변화의 강도와 인상도 크게 달라지며, 자신만의 디지털 아트를 발견하는 감각을 즐길 수 있습니다.',
    'ops-heading': '파티클을 조작하는<br />5가지 주요 방법',
    'op1-title': '인력', 'op1-key': '왼쪽 클릭',
    'op1-desc': '커서를 향해 파티클이 끌어당겨지는 조작입니다. 주변 파티클이 빨려들듯 모여 흐름에 통일감이 생깁니다.',
    'op2-title': '소용돌이', 'op2-key': 'V 키',
    'op2-desc': '커서 주변에 소용돌이 같은 회전을 만들어내는 조작입니다. 파티클을 끌어들이면서 흐름을 변화시키고 복잡한 파동을 만들어냅니다.',
    'op3-title': '타임스톱', 'op3-key': 'T 키',
    'op3-desc': '파티클을 급격히 감속시키는 조작입니다. 움직이던 파티클이 일제히 잠잠해지며 시간이 멈춘 듯한 인상적인 변화를 만들어냅니다.',
    'op4-title': '파문 발생', 'op4-key': 'B 키',
    'op4-desc': '한 점에서 충격파처럼 퍼져나가는 조작입니다. 중심에서 바깥쪽으로 파티클이 밀려나가며 파도 같은 움직임이 발생합니다.',
    'op5-title': '수속', 'op5-key': 'G 키',
    'op5-desc': '화면 위의 파티클을 한꺼번에 모으는 조작입니다. 흩어져 있던 파티클이 한데 모여 밀도 높은 다이나믹한 변화를 즐길 수 있습니다.',
    'ops-note': '※ 보기 편하도록 일부 파라미터를 설정했습니다',
    'ops-other-label': '기타 조작',
    'ops-other-restart': '재시작', 'ops-other-explode': '폭발', 'ops-other-param': '파라미터 설정 화면',
    'params-heading': '세부 설정',
    'params-desc': '강약, 범위, 속도 등의 파라미터를 조정할 수 있습니다.',
    'param1-title': '파티클 크기', 'param1-desc': '너무 작다고 느낄 때 사용해 주세요.',
    'param2-title': '파티클 수', 'param2-desc': '너무 많아 보기 어려울 때 사용해 주세요.',
    'param3-title': '색상 변경', 'param3-desc': '좋아하는 색으로 바꿔보세요!',
    'params-more': '더욱 세밀한 설정도 가능합니다 →',
    'rec-heading': '추천 플레이 방법',
    'rec1-title': '소용돌이로 끌어들여 모으기', 'rec1-subtitle': 'V 계속 누르기 ⇒ G 누르기 ⇒ G 떼기',
    'rec1-desc': '소용돌이를 발생시킨 후 파티클을 모으면 흐름이 크게 변화합니다. 참고 영상을 보면서 시도해 보세요.',
    'rec2-title': '순간의 변화를 즐기기', 'rec2-subtitle': 'G를 잠깐 누르고, E를 길게 누르기',
    'rec2-desc': '파티클의 움직임에 큰 변화가 생깁니다. 짧은 조작으로도 인상이 크게 달라지는 것이 특징입니다. G를 누르는 시간에 따라 보이는 것이 달라질 수도 있어요? G를 끝까지 누르다가 놓아보세요!',
    'rec3-title': '선 같은 표현 만들기', 'rec3-subtitle': 'G를 길게 누른 후 커서 움직이기',
    'rec3-desc': '선 같은 궤적이 나타납니다. 그 선에 왼쪽 클릭하면 또 다른 변화를 즐길 수 있습니다.',
    'rec4-title': '파라미터 바꿔보기', 'rec4-subtitle': 'speed limit를 200px/s로 변경',
    'rec4-desc': 'params를 열고 Physics → speed limit를 200px/s로 변경한 후 1, 2, 3을 다시 시도해 보세요. 같은 조작이어도 완전히 다른 광경이 펼쳐집니다.',
    'final-heading': '자신만의 움직임과<br />표현을 찾아보세요',
    'final-desc': '파라미터를 하나 바꾸거나 버튼을 누르는 시간과 타이밍만으로도 보이는 광경이 크게 달라집니다.',
    'footer-rights': '© 2026 Particle Flow', 'footer-tagline': 'Designed with intention',
  },
};

let currentLang = 'ja';

function setLanguage(lang) {
  currentLang = lang;
  const t = i18n[lang];
  document.documentElement.lang = lang;

  document.querySelectorAll('[data-i18n]').forEach(el => {
    const key = el.getAttribute('data-i18n');
    if (t[key] !== undefined) el.textContent = t[key];
  });

  document.querySelectorAll('[data-i18n-html]').forEach(el => {
    const key = el.getAttribute('data-i18n-html');
    if (t[key] !== undefined) el.innerHTML = t[key];
  });

  document.querySelectorAll('.lang-btn').forEach(btn => {
    btn.classList.toggle('active', btn.getAttribute('data-lang') === lang);
  });
}

function scrollToTop() {
  window.scrollTo({ top: 0, behavior: 'smooth' });
}

function scrollToContent() {
  window.scrollTo({ top: window.innerHeight, behavior: 'smooth' });
}

/* ---- Scroll reveal ---- */
const observer = new IntersectionObserver((entries) => {
  entries.forEach(entry => {
    if (entry.isIntersecting) {
      entry.target.classList.add('visible');
      observer.unobserve(entry.target);
    }
  });
}, { threshold: 0.12 });

document.querySelectorAll('.reveal, .reveal-fade').forEach(el => observer.observe(el));

/* ---- Lightbox ---- */
const lightbox    = document.getElementById('lightbox');
const lightboxImg = document.getElementById('lightbox-img');

function openLightbox(src, alt) {
  lightboxImg.src = src;
  lightboxImg.alt = alt;
  lightbox.classList.add('active');
  document.body.style.overflow = 'hidden';
}

function closeLightbox() {
  lightbox.classList.remove('active');
  document.body.style.overflow = '';
}

/* 画像クリック/タップで開く */
document.querySelectorAll('.param-img').forEach(img => {
  img.addEventListener('click', () => openLightbox(img.src, img.alt));
});

/* 背景クリックで閉じる */
lightbox.addEventListener('click', (e) => {
  if (e.target === lightbox || e.target.classList.contains('lightbox-bg')) {
    closeLightbox();
  }
});

/* Esc キーで閉じる */
document.addEventListener('keydown', (e) => {
  if (e.key === 'Escape') closeLightbox();
});

/* ---- Back to top ---- */
const backToTopBtn = document.getElementById('back-to-top');
window.addEventListener('scroll', () => {
  backToTopBtn.classList.toggle('visible', window.scrollY > window.innerHeight * 0.5);
}, { passive: true });
