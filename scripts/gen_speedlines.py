#!/usr/bin/env python3
"""Generates the NFS-Unbound-style nitro speed lines for hud/HUDSpeedometerControl.xaml and patches the XAML in place
(idempotent: it replaces what is between the SPEEDLINES markers). Run:  python3 scripts/gen_speedlines.py
                                                                         python3 scripts/gen_hud_data.py

How it works:
  * Every streak is a thin gradient rectangle in a TransformGroup [Scale, Translate(X = distance from the middle of the
    screen), Rotate(angle)], flown outward by its own looping XAML animation (own period, start delay, speed, fade).
  * The streaks are split into tiers of different speed (the animation periods differ). Each tier is a container
    whose opacity the plugin sets from the car's speed: neighbouring tiers are crossfaded, so as the car speeds up the
    lines get faster without a visible jump. The tier containers also sway slowly (a looping rotation of a few degrees)
    so a streak does not come back on exactly the same path every time.
  * The container opacities are data items of the speedometer: RPMGaugeNumbers[15].XPos, [15].YPos, [14].XPos, ...
    (the stock rev numbers 13..15 are removed from the XAML for it).
"""
import math, os, random, re
root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
path = os.path.join(root, 'hud', 'HUDSpeedometerControl.xaml')

# the km/h each tier is made for  -- keep in step with kSpeedLineTierKmh in nitro_mod.cpp
TIER_KMH = [40, 70, 110, 165, 235, 320]
PER_TIER = 10
CHANNELS = ['RPMGaugeNumbers[15].XPos', 'RPMGaugeNumbers[15].YPos', 'RPMGaugeNumbers[14].XPos',
            'RPMGaugeNumbers[14].YPos', 'RPMGaugeNumbers[13].XPos', 'RPMGaugeNumbers[13].YPos']
SWAY = 20            # +- degrees a tier container sways
SWAY_PERIODS = [13.0, 17.0, 11.0, 15.0, 19.0, 12.0]   # seconds
CONE = (52 - SWAY, 128 + SWAY)    # directions that stay empty: the car is straight down from the middle
rnd = random.Random(20261031)
CTX = 'DataContext="{Binding Path=DataContext, RelativeSource={RelativeSource AncestorType={x:Type UserControl}}}"'

def f(v): return ('%.3f' % v).rstrip('0').rstrip('.')

def edge_radius(deg):
    """distance from the middle of the 1920x1080 scene to its edge in this direction"""
    a = math.radians(deg)
    c, s = abs(math.cos(a)), abs(math.sin(a))
    return min(960 / c if c > 1e-6 else 1e9, 540 / s if s > 1e-6 else 1e9)

def pick_angle():
    while True:
        a = rnd.uniform(0, 360)
        if CONE[0] <= a <= CONE[1]:
            continue
        return a

streaks = []
for t, kmh in enumerate(TIER_KMH):
    speed = 8.0 * kmh      # px/s a streak of this tier averages on its way out (8 px/s per km/h; was 4 in 1.7.5)
    for i in range(PER_TIER):
        ang = pick_angle()
        R = edge_radius(ang)
        worst = max(edge_radius(ang + d) for d in range(-SWAY, SWAY + 1, 4))
        length = rnd.uniform(110, 300)
        r0 = R * rnd.uniform(0.52, 0.72)                       # a bit further in than the rim, not from the middle
        r1 = worst * 1.12 + length * 0.9                       # surely off screen at the end of the flight
        d = (r1 - r0) / (speed * rnd.uniform(0.75, 1.35))
        streaks.append(dict(tier=t, name='NitroSL%d_%d' % (t, i), angle=ang, length=length, thick=rnd.choice([2, 2, 2, 3, 3]),
                            r0=r0, r1=r1, dur=d, begin=rnd.uniform(0, d), peak=rnd.uniform(0.45, 0.85)))

TP = '(UIElement.RenderTransform).(TransformGroup.Children)'

def storyboard():
    out = ['            <!-- SPEEDLINES-BEGIN storyboard -->',
           '            <LinearGradientBrush x:Key="NitroSpeedLine_Brush" StartPoint="0,0.5" EndPoint="1,0.5">',
           '                <GradientStop Color="#004FD8FF" Offset="0"/>',
           '                <GradientStop Color="#704FD8FF" Offset="0.55"/>',
           '                <GradientStop Color="#FFB8F2FF" Offset="1"/>',
           '            </LinearGradientBrush>',
           '            <Storyboard x:Key="NitroSpeedLines">']
    for t in range(len(TIER_KMH)):     # the slow sway of each tier container
        T = SWAY_PERIODS[t]
        sign = 1 if t % 2 == 0 else -1
        out.append('                <DoubleAnimationUsingKeyFrames Storyboard.TargetName="NitroTier%d" Storyboard.TargetProperty="%s[2].(RotateTransform.Angle)" RepeatBehavior="Forever">' % (t, TP))
        for frac, val in ((0, 0), (0.25, SWAY * sign), (0.75, -SWAY * sign), (1.0, 0)):
            out.append('                    <EasingDoubleKeyFrame KeyTime="0:0:%s" Value="%s"><EasingDoubleKeyFrame.EasingFunction><SineEase EasingMode="EaseInOut"/></EasingDoubleKeyFrame.EasingFunction></EasingDoubleKeyFrame>' % (f(T * frac), f(val)))
        out.append('                </DoubleAnimationUsingKeyFrames>')
    for s in streaks:
        n = s['name']; d = s['dur']; b = '0:0:%s' % f(s['begin'])
        out.append('                <DoubleAnimationUsingKeyFrames Storyboard.TargetName="%s" Storyboard.TargetProperty="%s[1].(TranslateTransform.X)" RepeatBehavior="Forever" BeginTime="%s">' % (n, TP, b))
        out.append('                    <EasingDoubleKeyFrame KeyTime="0:0:0" Value="%s"/>' % f(s['r0']))
        out.append('                    <EasingDoubleKeyFrame KeyTime="0:0:%s" Value="%s"><EasingDoubleKeyFrame.EasingFunction><PowerEase EasingMode="EaseIn" Power="1.6"/></EasingDoubleKeyFrame.EasingFunction></EasingDoubleKeyFrame>' % (f(d), f(s['r1'])))
        out.append('                </DoubleAnimationUsingKeyFrames>')
        out.append('                <DoubleAnimationUsingKeyFrames Storyboard.TargetName="%s" Storyboard.TargetProperty="%s[0].(ScaleTransform.ScaleX)" RepeatBehavior="Forever" BeginTime="%s">' % (n, TP, b))
        out.append('                    <EasingDoubleKeyFrame KeyTime="0:0:0" Value="0.5"/>')
        out.append('                    <EasingDoubleKeyFrame KeyTime="0:0:%s" Value="1.6"/>' % f(d))
        out.append('                </DoubleAnimationUsingKeyFrames>')
        out.append('                <DoubleAnimationUsingKeyFrames Storyboard.TargetName="%s" Storyboard.TargetProperty="(UIElement.Opacity)" RepeatBehavior="Forever" BeginTime="%s">' % (n, b))
        out.append('                    <EasingDoubleKeyFrame KeyTime="0:0:0" Value="0"/>')
        out.append('                    <EasingDoubleKeyFrame KeyTime="0:0:%s" Value="%s"/>' % (f(d * 0.3), f(s['peak'] * 0.45)))
        out.append('                    <EasingDoubleKeyFrame KeyTime="0:0:%s" Value="%s"/>' % (f(d * 0.75), f(s['peak'])))
        out.append('                    <EasingDoubleKeyFrame KeyTime="0:0:%s" Value="0"/>' % f(d))
        out.append('                </DoubleAnimationUsingKeyFrames>')
    out.append('            </Storyboard>')
    out.append('            <!-- SPEEDLINES-END storyboard -->')
    return '\n'.join(out) + '\n'

def triggers():
    return '''    <!-- SPEEDLINES-BEGIN triggers -->
    <UserControl.Triggers>
        <EventTrigger RoutedEvent="FrameworkElement.Loaded">
            <BeginStoryboard Storyboard="{StaticResource NitroSpeedLines}"/>
        </EventTrigger>
    </UserControl.Triggers>
    <!-- SPEEDLINES-END triggers -->
'''

def elements():
    out = ['        <!-- SPEEDLINES-BEGIN elements -->',
           '        <!-- Nitro speed lines (like NFS Unbound), see scripts/gen_speedlines.py: streaks flying outward in %d tiers of different speed.' % len(TIER_KMH),
           '             They run all the time; the plugin sets the opacity of each tier container (%s)' % ', '.join(CHANNELS),
           '             from the nitro and the car speed (0 = invisible), crossfading neighbouring tiers. -->']
    for t in range(len(TIER_KMH)):
        out.append('        <Grid x:Name="NitroTier%d" HorizontalAlignment="Center" VerticalAlignment="Center" Width="2300" Height="1350" IsHitTestVisible="False" RenderTransformOrigin="0.5,0.5" %s' % (t, CTX))
        out.append('              Opacity="{Binding %s}">' % CHANNELS[t])
        out.append('            <Grid.RenderTransform>')
        out.append('                <TransformGroup><ScaleTransform/><SkewTransform/><RotateTransform/><TranslateTransform X="-693" Y="-362"/></TransformGroup>')
        out.append('            </Grid.RenderTransform>')
        for s in streaks:
            if s['tier'] != t:
                continue
            out.append('            <Rectangle x:Name="%s" Width="%s" Height="%d" HorizontalAlignment="Center" VerticalAlignment="Center" Opacity="0" RenderTransformOrigin="0.5,0.5" Fill="{StaticResource NitroSpeedLine_Brush}">' % (s['name'], f(s['length']), s['thick']))
            out.append('                <Rectangle.RenderTransform><TransformGroup><ScaleTransform ScaleX="1"/><TranslateTransform X="%s"/><RotateTransform Angle="%s"/></TransformGroup></Rectangle.RenderTransform>' % (f(s['r0']), f(s['angle'])))
            out.append('            </Rectangle>')
        out.append('        </Grid>')
    out.append('        <!-- SPEEDLINES-END elements -->')
    return '\n'.join(out) + '\n'

def patch(t):
    nl = '\r\n' if '\r\n' in t else '\n'
    t = t.replace('\r\n', '\n')
    t = re.sub(r'[ \t]*<!-- SPEEDLINES-BEGIN (\w+) -->.*?<!-- SPEEDLINES-END \1 -->\n', '', t, flags=re.S)
    a = t.index('            <RadialGradientBrush x:Key="A_Brush_MapShadowBackground">')
    t = t[:a] + storyboard() + '\n' + t[a:]
    a = t.index('</UserControl.Resources>') + len('</UserControl.Resources>')
    t = t[:a] + '\n' + triggers() + t[a:]
    key = '<ContentControl HorizontalAlignment="Center" VerticalAlignment="Center" Style="{StaticResource A_Style_ContentControl_Speedometer}"/>'
    a = t.index(key)
    a = t.rindex('\n', 0, a) + 1
    t = t[:a] + elements() + '\n' + t[a:]
    return t.replace('\n', nl)

REV = '                    <local:FZMaterial DataContext="{Binding RPMGaugeNumbers[%d]}" Style="{StaticResource A_Style_FZMaterial_GaugeNumbers}" ImageSource="/ForzaUI;component/Textures/Anthem/HUD/Speedometer/HUD_Dial_RevNumbers_%d.png"/>'

if __name__ == '__main__':
    t = open(path, encoding='utf-8', newline='').read()
    nl = '\r\n' if '\r\n' in t else '\n'
    # the stock rev numbers 13..15 are dropped (their data items carry the speed lines' opacity); 11 and 12 stay
    t = re.sub(r'[ \t]*<local:FZMaterial DataContext="\{Binding RPMGaugeNumbers\[1[3-5]\]\}"[^\n]*/>\r?\n', '', t)
    for n in (11, 12):
        if not re.search(r'<local:FZMaterial DataContext="\{Binding RPMGaugeNumbers\[%d\]\}" Style=' % n, t):
            prev = re.search(r'[ \t]*<local:FZMaterial DataContext="\{Binding RPMGaugeNumbers\[%d\]\}" Style=[^\n]*/>\r?\n' % (n - 1), t)
            t = t[:prev.end()] + nl + REV % (n, n) + nl + t[prev.end():]
    open(path, 'w', encoding='utf-8', newline='').write(patch(t))
    print('speed lines written:', len(streaks))
