import { Card } from "@/components/ui/card";
import { Code2, Zap, Users } from "lucide-react";

const About = () => {
  const highlights = [
    {
      icon: <Code2 className="w-6 h-6" />,
      title: "Clean Code",
      description: "Writing maintainable and scalable solutions"
    },
    {
      icon: <Zap className="w-6 h-6" />,
      title: "Performance",
      description: "Optimized for speed and efficiency"
    },
    {
      icon: <Users className="w-6 h-6" />,
      title: "Collaboration",
      description: "Team player with strong communication"
    },
  ];

  return (
    <section className="py-20 px-4" id="about">
      <div className="container mx-auto">
        <div className="grid grid-cols-1 lg:grid-cols-2 gap-12 items-center">
          <div>
            <h2 className="text-4xl md:text-5xl font-bold mb-6">
              About <span className="text-gradient">Me</span>
            </h2>
            <p className="text-lg text-muted-foreground mb-6">
              I'm a passionate fullstack developer with expertise in building modern applications 
              that deliver exceptional user experiences. My journey spans from creating beautiful 
              mobile interfaces with Flutter to architecting robust backend systems with Django and Golang.
            </p>
            <p className="text-lg text-muted-foreground mb-6">
              I hold a Bachelor's degree in Computer Science from one of Ethiopia's top universities, 
              where I built a strong foundation in software engineering principles and modern development practices.
            </p>
            <p className="text-lg text-muted-foreground mb-6">
              With Firebase powering real-time features and seamless integrations, I bring ideas 
              to life with cutting-edge technology. I believe in writing clean, maintainable code 
              and continuously learning new technologies to stay at the forefront of development.
            </p>
          </div>

          <div className="space-y-4">
            {highlights.map((item, index) => (
              <Card 
                key={index}
                className="glass p-6 hover:translate-x-2 transition-all duration-300"
              >
                <div className="flex items-start gap-4">
                  <div className="p-3 bg-gradient-to-br from-primary/20 to-accent/20 rounded-lg text-accent">
                    {item.icon}
                  </div>
                  <div>
                    <h3 className="text-xl font-semibold mb-2">{item.title}</h3>
                    <p className="text-muted-foreground">{item.description}</p>
                  </div>
                </div>
              </Card>
            ))}
          </div>
        </div>
      </div>
    </section>
  );
};

export default About;
